#include "flip_battery.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static int read_line_at(
    const char *root,
    const char *name,
    char *buffer,
    size_t buffer_size)
{
    if (!root ||
        !name ||
        !buffer ||
        buffer_size == 0)
        return -1;

    char path[512];

    int written = snprintf(
        path,
        sizeof(path),
        "%s/%s",
        root,
        name);

    if (written < 0 ||
        (size_t)written >= sizeof(path))
        return -1;

    FILE *f =
        fopen(
            path,
            "r");

    if (!f)
        return -1;

    if (!fgets(
            buffer,
            (int)buffer_size,
            f)) {
        fclose(f);
        return -1;
    }

    fclose(f);

    char *newline =
        strpbrk(
            buffer,
            "\r\n");

    if (newline)
        *newline = '\0';

    return 0;
}

static int read_long_at(
    const char *root,
    const char *name,
    long *value)
{
    if (!value)
        return -1;

    char buffer[128];

    if (read_line_at(
            root,
            name,
            buffer,
            sizeof(buffer)) < 0)
        return -1;

    errno = 0;
    char *end = NULL;

    long parsed =
        strtol(
            buffer,
            &end,
            10);

    if (errno != 0 ||
        end == buffer ||
        *end != '\0')
        return -1;

    *value = parsed;
    return 0;
}

static int read_line(
    const char *name,
    char *buffer,
    size_t buffer_size)
{
    return read_line_at(
        FLIP_BATTERY_ROOT,
        name,
        buffer,
        buffer_size);
}

static int read_long(
    const char *name,
    long *value)
{
    return read_long_at(
        FLIP_BATTERY_ROOT,
        name,
        value);
}

static flip_battery_status_t parse_status(
    const char *text)
{
    if (!text)
        return FLIP_BATTERY_STATUS_UNKNOWN;

    if (strcasecmp(
            text,
            "Charging") == 0)
        return FLIP_BATTERY_STATUS_CHARGING;

    if (strcasecmp(
            text,
            "Discharging") == 0)
        return FLIP_BATTERY_STATUS_DISCHARGING;

    if (strcasecmp(
            text,
            "Not charging") == 0)
        return FLIP_BATTERY_STATUS_NOT_CHARGING;

    if (strcasecmp(
            text,
            "Full") == 0)
        return FLIP_BATTERY_STATUS_FULL;

    return FLIP_BATTERY_STATUS_UNKNOWN;
}

static flip_battery_capacity_level_t parse_capacity_level(
    const char *text)
{
    if (!text)
        return FLIP_BATTERY_CAPACITY_UNKNOWN;

    if (strcasecmp(text, "Critical") == 0)
        return FLIP_BATTERY_CAPACITY_CRITICAL;
    if (strcasecmp(text, "Low") == 0)
        return FLIP_BATTERY_CAPACITY_LOW;
    if (strcasecmp(text, "Normal") == 0)
        return FLIP_BATTERY_CAPACITY_NORMAL;
    if (strcasecmp(text, "High") == 0)
        return FLIP_BATTERY_CAPACITY_HIGH;
    if (strcasecmp(text, "Full") == 0)
        return FLIP_BATTERY_CAPACITY_FULL;

    return FLIP_BATTERY_CAPACITY_UNKNOWN;
}

static void read_external_power(
    flip_battery_snapshot_t *snapshot)
{
    long ac_online = 0;
    long usb_online = 0;
    int ac_valid =
        read_long_at(FLIP_AC_ROOT, "online", &ac_online) == 0 &&
        (ac_online == 0 || ac_online == 1);
    int usb_valid =
        read_long_at(FLIP_USB_ROOT, "online", &usb_online) == 0 &&
        (usb_online == 0 || usb_online == 1);

    if (!ac_valid && !usb_valid)
        return;

    snapshot->external_power_valid = 1;
    snapshot->external_power_online =
        (ac_valid && ac_online == 1) ||
        (usb_valid && usb_online == 1);

    FILE *override = fopen(FLIP_EXTERNAL_POWER_OVERRIDE, "r");
    if (override) {
        long value = -1;
        if (fscanf(override, "%ld", &value) == 1 &&
            (value == 0 || value == 1)) {
            snapshot->external_power_valid = 1;
            snapshot->external_power_online = (int)value;
        }
        fclose(override);
    }
}

static int read_dev_override_long(
    const char *path,
    long minimum,
    long maximum,
    long *value)
{
    if (!path || !value)
        return 0;

    FILE *f = fopen(path, "r");
    if (!f)
        return 0;

    long parsed = 0;
    int valid = fscanf(f, "%ld", &parsed) == 1 &&
        parsed >= minimum && parsed <= maximum;
    fclose(f);

    if (!valid)
        return 0;

    *value = parsed;
    return 1;
}

int flip_battery_read(
    flip_battery_snapshot_t *snapshot)
{
    if (!snapshot)
        return -1;

    memset(
        snapshot,
        0,
        sizeof(*snapshot));

    snapshot->status =
        FLIP_BATTERY_STATUS_UNKNOWN;
    snapshot->capacity_level =
        FLIP_BATTERY_CAPACITY_UNKNOWN;

    long value = 0;

    if (read_long(
            "capacity",
            &value) == 0 &&
        value >= 0 &&
        value <= 100) {

        snapshot->percent =
            (int)value;
        snapshot->percent_valid = 1;
    }

    char status[64];

    if (read_line(
            "status",
            status,
            sizeof(status)) == 0) {

        flip_battery_status_t parsed =
            parse_status(status);

        if (parsed !=
            FLIP_BATTERY_STATUS_UNKNOWN) {
            snapshot->status = parsed;
            snapshot->status_valid = 1;
        }
    }

    if (read_long(
            "voltage_now",
            &value) == 0 &&
        value > 0 &&
        value <= 10000000L) {

        snapshot->voltage_uv = value;
        snapshot->voltage_valid = 1;
    }

    if (read_long(
            "current_now",
            &value) == 0 &&
        value >= -10000000L &&
        value <= 10000000L) {

        snapshot->current_ua = value;
        snapshot->current_valid = 1;
    }

    if (read_long(
            "temp",
            &value) == 0 &&
        value >= -400 &&
        value <= 1000) {

        snapshot->temp_deci_c =
            (int)value;
        snapshot->temp_valid = 1;
    }

    /* Development-only RAM overrides for real-hardware policy validation. */
    long override_value = 0;
    if (read_dev_override_long(
            FLIP_BATTERY_PERCENT_OVERRIDE, 0, 100, &override_value)) {
        snapshot->percent = (int)override_value;
        snapshot->percent_valid = 1;
    }

    if (read_dev_override_long(
            FLIP_BATTERY_VOLTAGE_OVERRIDE, 2500000L, 5000000L,
            &override_value)) {
        snapshot->voltage_uv = override_value;
        snapshot->voltage_valid = 1;
    }

    if (read_dev_override_long(
            FLIP_BATTERY_TEMP_OVERRIDE, -400, 1000, &override_value)) {
        snapshot->temp_deci_c = (int)override_value;
        snapshot->temp_valid = 1;
    }

    char capacity_level[64];
    if (read_line(
            "capacity_level",
            capacity_level,
            sizeof(capacity_level)) == 0) {
        flip_battery_capacity_level_t parsed =
            parse_capacity_level(capacity_level);
        if (parsed != FLIP_BATTERY_CAPACITY_UNKNOWN) {
            snapshot->capacity_level = parsed;
            snapshot->capacity_level_valid = 1;
        }
    }

    read_external_power(snapshot);

    return 0;
}

const char *flip_battery_status_name(
    flip_battery_status_t status)
{
    switch (status) {
    case FLIP_BATTERY_STATUS_CHARGING:
        return "CHARGING";
    case FLIP_BATTERY_STATUS_DISCHARGING:
        return "DISCHARGING";
    case FLIP_BATTERY_STATUS_NOT_CHARGING:
        return "NOT_CHARGING";
    case FLIP_BATTERY_STATUS_FULL:
        return "FULL";
    case FLIP_BATTERY_STATUS_UNKNOWN:
    default:
        return "UNKNOWN";
    }
}

const char *flip_battery_capacity_level_name(
    flip_battery_capacity_level_t level)
{
    switch (level) {
    case FLIP_BATTERY_CAPACITY_CRITICAL:
        return "CRITICAL";
    case FLIP_BATTERY_CAPACITY_LOW:
        return "LOW";
    case FLIP_BATTERY_CAPACITY_NORMAL:
        return "NORMAL";
    case FLIP_BATTERY_CAPACITY_HIGH:
        return "HIGH";
    case FLIP_BATTERY_CAPACITY_FULL:
        return "FULL";
    case FLIP_BATTERY_CAPACITY_UNKNOWN:
    default:
        return "UNKNOWN";
    }
}
