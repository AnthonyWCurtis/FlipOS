#include "flip_wifi.h"
#include "flip_wifi_profiles.h"

#include <stdio.h>
#include <string.h>

static void usage(const char *program)
{
    fprintf(stderr, "usage: %s status|scan|saved|forget <ssid>|disconnect\n", program);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        usage(argv[0]);
        return 2;
    }

    if (strcmp(argv[1], "status") == 0) {
        flip_wifi_status_t status;
        if (flip_wifi_get_status(&status) < 0)
            return 1;

        printf("interface_present=%d\n", status.interface_present);
        printf("enabled=%d\n", status.enabled);
        printf("connected=%d\n", status.connected);
        printf("usable_ip=%d\n", status.usable_ip);
        printf("ssid=%s\n", status.ssid);
        printf("ip_address=%s\n", status.ip_address);
        return 0;
    }

    if (strcmp(argv[1], "scan") == 0) {
        flip_wifi_network_t networks[FLIP_WIFI_MAX_NETWORKS];
        int count = flip_wifi_scan(networks, FLIP_WIFI_MAX_NETWORKS);
        if (count < 0)
            return 1;

        printf("count=%d\n", count);
        for (int i = 0; i < count; ++i) {
            printf(
                "network=%d\tsignal=%d\tsecure=%d\tsaved=%d\tssid=%s\n",
                i,
                networks[i].signal_dbm,
                networks[i].secure,
                networks[i].saved,
                networks[i].ssid);
        }
        return 0;
    }

    if (strcmp(argv[1], "saved") == 0) {
        flip_wifi_saved_profile_t profiles[FLIP_WIFI_SAVED_LIMIT];
        int count = flip_wifi_profiles_load(profiles);
        if (count < 0)
            return 1;
        printf("saved_count=%d\n", count);
        for (int i = 0; i < count; ++i) {
            printf("saved=%d\tpriority=%d\tsecure=%d\tssid=%s\n",
                i, profiles[i].priority, !profiles[i].open_network,
                profiles[i].ssid);
        }
        return 0;
    }

    if (strcmp(argv[1], "forget") == 0 && argc == 3)
        return flip_wifi_forget_network(argv[2]) == 0 ? 0 : 1;

    if (strcmp(argv[1], "disconnect") == 0)
        return flip_wifi_disconnect() == 0 ? 0 : 1;

    usage(argv[0]);
    return 2;
}
