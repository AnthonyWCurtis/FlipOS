#ifndef FLIP_WIFI_H
#define FLIP_WIFI_H

#include <stddef.h>

#include "flip_paths.h"

#ifndef FLIP_WIFI_INTERFACE
#define FLIP_WIFI_INTERFACE "wlan0"
#endif

#ifndef FLIP_WIFI_CONFIG_DIR
#define FLIP_WIFI_CONFIG_DIR FLIP_STATE_DATA_ROOT "/network"
#endif

#ifndef FLIP_WIFI_CONFIG_FILE
#define FLIP_WIFI_CONFIG_FILE FLIP_WIFI_CONFIG_DIR "/wpa_supplicant.conf"
#endif

#ifndef FLIP_WIFI_WPA_SUPPLICANT
#define FLIP_WIFI_WPA_SUPPLICANT "/usr/sbin/wpa_supplicant"
#endif

#ifndef FLIP_WIFI_WPA_CLI
#define FLIP_WIFI_WPA_CLI "/usr/sbin/wpa_cli"
#endif

#ifndef FLIP_WIFI_WPA_PASSPHRASE
#define FLIP_WIFI_WPA_PASSPHRASE "/usr/sbin/wpa_passphrase"
#endif

#ifndef FLIP_WIFI_IP
#define FLIP_WIFI_IP "/usr/sbin/ip"
#endif

#ifndef FLIP_WIFI_UDHCPC
#define FLIP_WIFI_UDHCPC "/usr/sbin/udhcpc"
#endif

#ifndef FLIP_WIFI_DHCPCD
#define FLIP_WIFI_DHCPCD "/usr/sbin/dhcpcd"
#endif

#ifndef FLIP_WIFI_UDHCPC_PIDFILE
#define FLIP_WIFI_UDHCPC_PIDFILE "/var/run/flipos-udhcpc-wlan0.pid"
#endif

#define FLIP_WIFI_MAX_NETWORKS 64
#define FLIP_WIFI_SSID_MAX 64

#define FLIP_WIFI_CONNECT_OK 0
#define FLIP_WIFI_CONNECT_AUTH_NO_IP 1
#define FLIP_WIFI_CONNECT_ERROR -1

typedef struct {
    char ssid[FLIP_WIFI_SSID_MAX];
    int signal_dbm;
    int secure;
    int saved;
    char flags[96];
} flip_wifi_network_t;

typedef struct {
    int interface_present;
    int enabled;
    int connected;
    int usable_ip;
    char ssid[FLIP_WIFI_SSID_MAX];
    char ip_address[64];
} flip_wifi_status_t;

int flip_wifi_parse_scan_results(
    const char *text,
    flip_wifi_network_t *networks,
    int max_networks);

int flip_wifi_write_config(
    const char *ssid,
    const char *passphrase);

int flip_wifi_scan(
    flip_wifi_network_t *networks,
    int max_networks);

int flip_wifi_connect(
    const char *ssid,
    const char *passphrase);

int flip_wifi_disconnect(void);

int flip_wifi_forget(void);

/* Use stored credentials without displaying or retyping the password. */
int flip_wifi_connect_saved(const char *ssid);
int flip_wifi_network_is_saved(const char *ssid);

/* Remove only the selected SSID; preserve all other saved profiles. */
int flip_wifi_forget_network(const char *ssid);

int flip_wifi_get_status(
    flip_wifi_status_t *status);

#endif
