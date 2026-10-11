#include "flip_wifi.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Public saved-network management API is part of the v1 contract. */
static int (*forget_api)(void) = flip_wifi_forget;

static void test_scan_parser(void)
{
    const char *scan =
        "bssid / frequency / signal level / flags / ssid\n"
        "00:11:22:33:44:55\t2412\t-62\t[WPA2-PSK-CCMP][ESS]\tHome WiFi\n"
        "00:11:22:33:44:66\t5180\t-47\t[WPA2-PSK-CCMP][ESS]\tHome WiFi\n"
        "aa:bb:cc:dd:ee:ff\t2437\t-51\t[ESS]\tGuest\n"
        "aa:bb:cc:dd:ee:00\t2462\t-80\t[WPA2-PSK-CCMP][ESS]\tOther\n";

    flip_wifi_network_t networks[8];
    int count = flip_wifi_parse_scan_results(scan, networks, 8);
    assert(count == 3);
    assert(strcmp(networks[0].ssid, "Home WiFi") == 0);
    assert(networks[0].signal_dbm == -47);
    assert(networks[0].secure);
    assert(strcmp(networks[1].ssid, "Guest") == 0);
    assert(!networks[1].secure);
    assert(strcmp(networks[2].ssid, "Other") == 0);
}

static void test_config_writer(void)
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

    assert(flip_wifi_write_config("My \"WiFi\"", "password123") == 0);

    FILE *f = fopen(FLIP_WIFI_CONFIG_FILE, "r");
    assert(f != NULL);
    char data[2048];
    size_t got = fread(data, 1, sizeof(data) - 1, f);
    fclose(f);
    data[got] = '\0';

    assert(strstr(data, "ssid=\"My \\\"WiFi\\\"\"") != NULL);
    assert(strstr(data,
        "psk=0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef") != NULL);
    assert(strstr(data, "password123") == NULL);
    assert(strstr(data, "plaintext-must-not-be-saved") == NULL);

    struct stat st;
    assert(stat(FLIP_WIFI_CONFIG_FILE, &st) == 0);
    assert((st.st_mode & 0777) == 0600);

    assert(flip_wifi_write_config("Cafe", NULL) == 0);
    f = fopen(FLIP_WIFI_CONFIG_FILE, "r");
    assert(f != NULL);
    got = fread(data, 1, sizeof(data) - 1, f);
    fclose(f);
    data[got] = '\0';
    assert(strstr(data, "key_mgmt=NONE") != NULL);
    assert(strstr(data, "psk=") == NULL);

    assert(flip_wifi_write_config("Bad\nSSID", "password123") < 0);
    assert(flip_wifi_write_config("Good", "short") < 0);
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
    test_config_writer();
    test_connect_result_constants();
    puts("flip_wifi tests passed");
    return EXIT_SUCCESS;
}
