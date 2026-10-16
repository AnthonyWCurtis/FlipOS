#ifndef FLIP_BATTERY_H
#define FLIP_BATTERY_H

#ifndef FLIP_BATTERY_ROOT
#define FLIP_BATTERY_ROOT "/sys/class/power_supply/battery"
#endif

#ifndef FLIP_AC_ROOT
#define FLIP_AC_ROOT "/sys/class/power_supply/ac"
#endif

#ifndef FLIP_USB_ROOT
#define FLIP_USB_ROOT "/sys/class/power_supply/usb"
#endif

#ifndef FLIP_BATTERY_TEMP_OVERRIDE
#define FLIP_BATTERY_TEMP_OVERRIDE "/tmp/flipos-dev-battery-temp-override"
#endif

#ifndef FLIP_BATTERY_PERCENT_OVERRIDE
#define FLIP_BATTERY_PERCENT_OVERRIDE "/tmp/flipos-dev-battery-percent-override"
#endif

#ifndef FLIP_BATTERY_VOLTAGE_OVERRIDE
#define FLIP_BATTERY_VOLTAGE_OVERRIDE "/tmp/flipos-dev-battery-voltage-override"
#endif

#ifndef FLIP_EXTERNAL_POWER_OVERRIDE
#define FLIP_EXTERNAL_POWER_OVERRIDE "/tmp/flipos-dev-external-power-override"
#endif

typedef enum {
    FLIP_BATTERY_STATUS_UNKNOWN = 0,
    FLIP_BATTERY_STATUS_CHARGING,
    FLIP_BATTERY_STATUS_DISCHARGING,
    FLIP_BATTERY_STATUS_NOT_CHARGING,
    FLIP_BATTERY_STATUS_FULL
} flip_battery_status_t;

typedef enum {
    FLIP_BATTERY_CAPACITY_UNKNOWN = 0,
    FLIP_BATTERY_CAPACITY_CRITICAL,
    FLIP_BATTERY_CAPACITY_LOW,
    FLIP_BATTERY_CAPACITY_NORMAL,
    FLIP_BATTERY_CAPACITY_HIGH,
    FLIP_BATTERY_CAPACITY_FULL
} flip_battery_capacity_level_t;

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

    flip_battery_capacity_level_t capacity_level;
    int capacity_level_valid;

    int external_power_online;
    int external_power_valid;
} flip_battery_snapshot_t;

int flip_battery_read(
    flip_battery_snapshot_t *snapshot);

/*
 * Physical-sensor snapshot for charger supervision. No /tmp development
 * override may influence temperature or external-power charging decisions.
 */
int flip_battery_read_hardware(
    flip_battery_snapshot_t *snapshot);

const char *flip_battery_status_name(
    flip_battery_status_t status);

const char *flip_battery_capacity_level_name(
    flip_battery_capacity_level_t level);

#endif
