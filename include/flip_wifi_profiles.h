#ifndef FLIP_WIFI_PROFILES_H
#define FLIP_WIFI_PROFILES_H

#include "flip_wifi.h"

#define FLIP_WIFI_SAVED_LIMIT 16

typedef struct {
    char ssid[FLIP_WIFI_SSID_MAX];
    char psk[65];
    int open_network;
    int priority;
} flip_wifi_saved_profile_t;

/* Load returns count (0 means no profiles), -1 on invalid/unsafe config. */
int flip_wifi_profiles_load(
    flip_wifi_saved_profile_t profiles[FLIP_WIFI_SAVED_LIMIT]);

/* Replace the complete managed network set atomically (0600 file). */
int flip_wifi_profiles_store(
    const flip_wifi_saved_profile_t *profiles,
    int count);

/* Add/update credentials and put the selected profile first. */
int flip_wifi_profiles_upsert(
    const char *ssid,
    const char *hex_psk,
    int open_network);

/* Remove only one SSID, leaving other saved networks intact. */
int flip_wifi_profiles_forget(const char *ssid);

/* Make an already-saved, successfully joined SSID the highest priority. */
int flip_wifi_profiles_promote(const char *ssid);

/* Report presence without revealing PSK to the frontend. */
int flip_wifi_profiles_contains(const char *ssid);

#endif
