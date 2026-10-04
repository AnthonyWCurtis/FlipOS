#include "flip_battery.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static int read_line(
    const char *name,
    char *buffer,
    size_t buffer_size)
{
    if (!name ||
        !buffer ||
        buffer_size == 0)
        return -1;

    char path[512];

    int written = snprintf(
        path,
        sizeof(path),
        "%s/%s",
        FLIP_BATTERY_ROOT,
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

static int read_long(
    const char *name,
    long *value)
{
    if (!value)
        return -1;

    char buffer[128];

    if (read_line(
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
