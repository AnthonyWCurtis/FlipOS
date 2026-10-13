#include "flip_wifi.h"
#include "flip_wifi_profiles.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int (*forget_api)(void) = flip_wifi_forget;

static void reset_store(void)
{
    assert(system("rm -rf /tmp/flipos-wifi-test && mkdir -p /tmp/flipos-wifi-test") == 0);

    FILE *fake_passphrase = fopen(FLIP_WIFI_WPA_PASSPHRASE, "w");
    assert(fake_passphrase != NULL);
    assert(fputs(
        "#!/bin/sh\n"
        "IFS= read -r password\n"
        "echo network={\n"
        "echo '#psk=\"plaintext-must-not-be-saved\"'\n"
        "echo 'psk=0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef'\n"
        "echo }\n",
        fake_passphrase) >= 0);
    fclose(fake_passphrase);
    assert(chmod(FLIP_WIFI_WPA_PASSPHRASE, 0700) == 0);
}

static void read_config(char *out, size_t size)
{
    FILE *f = fopen(FLIP_WIFI_CONFIG_FILE, "r");
    assert(f != NULL);
    size_t got = fread(out, 1, size - 1, f);
    assert(!ferror(f) && !(!feof(f) && got == size - 1));
    assert(fclose(f) == 0);
    out[got] = '\0';
}

static int occurrences(const char *text, const char *needle)
{
    int count = 0;
    while ((text = strstr(text, needle)) != NULL) {
        ++count;
        text += strlen(needle);
    }
    return count;
}

static void test_scan_parser(void)
{
    const char *scan =
        "bssid / frequency / signal level / flags / ssid\n"
        "00:11:22:33:44:55\t2412\t-62\t[WPA2-PSK-CCMP][ESS]\tHome WiFi\n"
        "00:11:22:33:44:66\t5180\t-47\t[WPA2-PSK-CCMP][ESS]\tHome WiFi\n"
        "aa:bb:cc:dd:ee:ff\t2437\t-51\t[ESS]\tGuest\n"
        "aa:bb:cc:dd:ee:00\t2462\t-80\t[WPA2-PSK-CCMP][ESS]\tOther\n"
        "de:ad:be:ef:00:01\t2412\t-45\t[WPA2-PSK-CCMP][ESS]\tAnthony\\xe2\\x80\\x99s iPhone\n";

    flip_wifi_network_t networks[8];
    int count = flip_wifi_parse_scan_results(scan, networks, 8);
    assert(count == 4);
    assert(strcmp(
        networks[0].ssid,
        "Anthony" "\xE2\x80\x99" "s iPhone") == 0);
    assert(networks[0].signal_dbm == -45);
    assert(networks[0].secure);
    assert(networks[0].saved == 0);
    assert(strcmp(networks[1].ssid, "Home WiFi") == 0);
    assert(networks[1].signal_dbm == -47);
    assert(networks[1].secure);
    assert(strcmp(networks[2].ssid, "Guest") == 0);
    assert(!networks[2].secure);
    assert(strcmp(networks[3].ssid, "Other") == 0);
}

static void test_multiple_saved_networks(void)
{
    reset_store();
    assert(flip_wifi_write_config("Home WiFi", "password123") == 0);
    assert(flip_wifi_write_config("My \"iPhone\"\\ Hotspot", "password456") == 0);
    assert(flip_wifi_write_config("Cafe", NULL) == 0);

    flip_wifi_saved_profile_t profiles[FLIP_WIFI_SAVED_LIMIT];
    int count = flip_wifi_profiles_load(profiles);
    assert(count == 3);
    assert(strcmp(profiles[0].ssid, "Cafe") == 0);
    assert(strcmp(profiles[1].ssid, "My \"iPhone\"\\ Hotspot") == 0);
    assert(strcmp(profiles[2].ssid, "Home WiFi") == 0);
    assert(profiles[0].open_network == 1);
    assert(profiles[1].open_network == 0);
    assert(profiles[0].priority > profiles[1].priority);
    assert(profiles[1].priority > profiles[2].priority);

    char data[16000];
    read_config(data, sizeof(data));
    assert(occurrences(data, "network={") == 3);
    assert(strstr(data, "ssid=\"Home WiFi\"") != NULL);
    assert(strstr(data, "ssid=\"My \\\"iPhone\\\"\\\\ Hotspot\"") != NULL);
    assert(strstr(data, "ssid=\"Cafe\"") != NULL);
    assert(occurrences(data, "key_mgmt=NONE") == 1);
    assert(occurrences(data, "psk=") == 2);
    assert(strstr(data, "password123") == NULL);
    assert(strstr(data, "password456") == NULL);
    assert(strstr(data, "plaintext-must-not-be-saved") == NULL);

    struct stat st;
    assert(stat(FLIP_WIFI_CONFIG_FILE, &st) == 0);
    assert((st.st_mode & 0777) == 0600);

    assert(flip_wifi_profiles_contains("Home WiFi") == 1);
    assert(flip_wifi_network_is_saved("Cafe") == 1);
    assert(flip_wifi_network_is_saved("Unknown") == 0);

    assert(flip_wifi_profiles_promote("Home WiFi") == 0);
    count = flip_wifi_profiles_load(profiles);
    assert(count == 3);
    assert(strcmp(profiles[0].ssid, "Home WiFi") == 0);
    assert(strcmp(profiles[1].ssid, "Cafe") == 0);
    assert(strcmp(profiles[2].ssid, "My \"iPhone\"\\ Hotspot") == 0);

    assert(flip_wifi_write_config("Cafe", NULL) == 0);
    count = flip_wifi_profiles_load(profiles);
    assert(count == 3);
    assert(strcmp(profiles[0].ssid, "Cafe") == 0);

    assert(flip_wifi_forget_network("My \"iPhone\"\\ Hotspot") == 0);
    count = flip_wifi_profiles_load(profiles);
    assert(count == 2);
    assert(strcmp(profiles[0].ssid, "Cafe") == 0);
    assert(strcmp(profiles[1].ssid, "Home WiFi") == 0);
    assert(flip_wifi_forget_network("Does not exist") == -1);
    assert(flip_wifi_network_is_saved("Home WiFi") == 1);

    assert(flip_wifi_forget_network("Home WiFi") == 0);
    assert(flip_wifi_forget_network("Cafe") == 0);
    assert(flip_wifi_profiles_load(profiles) == 0);
}

static void test_legacy_migration_and_validation(void)
{
    reset_store();
    assert(flip_wifi_write_config("Initial", NULL) == 0);
    FILE *f = fopen(FLIP_WIFI_CONFIG_FILE, "w");
    assert(f != NULL);
    assert(fputs(
        "ctrl_interface=/var/run/wpa_supplicant\n"
        "update_config=0\n"
        "network={\n"
        "    ssid=\"Legacy Home\"\n"
        "    psk=0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\n"
        "}\n", f) >= 0);
    assert(fclose(f) == 0);

    assert(flip_wifi_write_config("Hotspot", "password123") == 0);
    flip_wifi_saved_profile_t profiles[FLIP_WIFI_SAVED_LIMIT];
    assert(flip_wifi_profiles_load(profiles) == 2);
    assert(strcmp(profiles[0].ssid, "Hotspot") == 0);
    assert(strcmp(profiles[1].ssid, "Legacy Home") == 0);

    char original[16000], updated[16000];
    read_config(original, sizeof(original));
    assert(flip_wifi_write_config("Bad\nSSID", "password123") < 0);
    assert(flip_wifi_write_config("Good", "short") < 0);
    assert(flip_wifi_write_config("Another", "password123\n") < 0);
    read_config(updated, sizeof(updated));
    assert(strcmp(original, updated) == 0);

    /* Unknown WPA directives are never silently lost on a rewrite. */
    f = fopen(FLIP_WIFI_CONFIG_FILE, "a");
    assert(f != NULL);
    assert(fputs("country=GB\n", f) >= 0);
    assert(fclose(f) == 0);
    read_config(original, sizeof(original));
    assert(flip_wifi_write_config("Another", NULL) == -1);
    read_config(updated, sizeof(updated));
    assert(strcmp(original, updated) == 0);
}

static void test_storage_limit(void)
{
    reset_store();
    for (int i = 0; i < FLIP_WIFI_SAVED_LIMIT; ++i) {
        char ssid[64];
        snprintf(ssid, sizeof(ssid), "Network %d", i);
        assert(flip_wifi_write_config(ssid, NULL) == 0);
    }
    assert(flip_wifi_write_config("Seventeenth", NULL) == -1);
    flip_wifi_saved_profile_t profiles[FLIP_WIFI_SAVED_LIMIT];
    assert(flip_wifi_profiles_load(profiles) == FLIP_WIFI_SAVED_LIMIT);
    assert(flip_wifi_network_is_saved("Network 0") == 1);
}

static void test_connect_result_constants(void)
{
    assert(strlen(FLIP_WIFI_UDHCPC_PIDFILE) > 0);
    assert(FLIP_WIFI_CONNECT_OK == 0);
    assert(FLIP_WIFI_CONNECT_AUTH_NO_IP == 1);
    assert(FLIP_WIFI_CONNECT_ERROR == -1);
}

int main(void)
{
    assert(forget_api != NULL);
    test_scan_parser();
    test_multiple_saved_networks();
    test_legacy_migration_and_validation();
    test_storage_limit();
    test_connect_result_constants();
    puts("flip_wifi tests passed");
    return EXIT_SUCCESS;
}
