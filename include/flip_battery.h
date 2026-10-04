#ifndef FLIP_BATTERY_H
#define FLIP_BATTERY_H

#ifndef FLIP_BATTERY_ROOT
#define FLIP_BATTERY_ROOT "/sys/class/power_supply/battery"
#endif

typedef enum {
    FLIP_BATTERY_STATUS_UNKNOWN = 0,
    FLIP_BATTERY_STATUS_CHARGING,
    FLIP_BATTERY_STATUS_DISCHARGING,
    FLIP_BATTERY_STATUS_NOT_CHARGING,
    FLIP_BATTERY_STATUS_FULL
} flip_battery_status_t;

typedef struct {
    int percent;
    int percent_valid;

    flip_battery_status_t status;
    int status_valid;

    long voltage_uv;
    int voltage_valid;

    long current_ua;
    int current_valid;

    int temp_deci_c;
    int temp_valid;
} flip_battery_snapshot_t;

int flip_battery_read(
    flip_battery_snapshot_t *snapshot);

const char *flip_battery_status_name(
    flip_battery_status_t status);

#endif
