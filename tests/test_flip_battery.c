#include "flip_battery.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void write_value_at(
    const char *root,
    const char *name,
    const char *value)
{
    char path[512];

    snprintf(
        path,
        sizeof(path),
        "%s/%s",
        root,
        name);

    FILE *f =
        fopen(
            path,
            "w");

    assert(f != NULL);
    assert(fputs(value, f) >= 0);
    fclose(f);
}

static void write_value(
    const char *name,
    const char *value)
{
    write_value_at(FLIP_BATTERY_ROOT, name, value);
}

static void remove_value(
    const char *name)
{
    char path[512];

    snprintf(
        path,
        sizeof(path),
        "%s/%s",
        FLIP_BATTERY_ROOT,
        name);

    unlink(path);
}

static void reset_root(void)
{
    unlink(FLIP_BATTERY_TEMP_OVERRIDE);
    unlink(FLIP_BATTERY_PERCENT_OVERRIDE);
    unlink(FLIP_BATTERY_VOLTAGE_OVERRIDE);
    unlink(FLIP_EXTERNAL_POWER_OVERRIDE);
    mkdir(FLIP_BATTERY_ROOT, 0700);
    mkdir(FLIP_AC_ROOT, 0700);
    mkdir(FLIP_USB_ROOT, 0700);

    const char *names[] = {
        "capacity",
        "status",
        "voltage_now",
        "current_now",
        "temp",
        "capacity_level",
    };

    for (size_t i = 0;
         i < sizeof(names) / sizeof(names[0]);
         ++i) {
        remove_value(names[i]);
    }

    char ac_path[512];
    char usb_path[512];
    snprintf(ac_path, sizeof(ac_path), "%s/online", FLIP_AC_ROOT);
    snprintf(usb_path, sizeof(usb_path), "%s/online", FLIP_USB_ROOT);
    unlink(ac_path);
    unlink(usb_path);
}

static void test_missing_is_nonfatal(void)
{
    reset_root();

    flip_battery_snapshot_t snapshot;

    assert(flip_battery_read(&snapshot) == 0);
    assert(!snapshot.percent_valid);
    assert(!snapshot.status_valid);
    assert(!snapshot.voltage_valid);
    assert(!snapshot.current_valid);
    assert(!snapshot.temp_valid);
    assert(!snapshot.capacity_level_valid);
    assert(!snapshot.external_power_valid);
    assert(snapshot.status ==
           FLIP_BATTERY_STATUS_UNKNOWN);
}

static void test_complete_snapshot(void)
{
    reset_root();

    write_value("capacity", "73\n");
    write_value("status", "Discharging\n");
    write_value("voltage_now", "4012000\n");
    write_value("current_now", "-321000\n");
    write_value("temp", "245\n");
    write_value("capacity_level", "Normal\n");
    write_value_at(FLIP_AC_ROOT, "online", "0\n");
    write_value_at(FLIP_USB_ROOT, "online", "1\n");

    flip_battery_snapshot_t snapshot;

    assert(flip_battery_read(&snapshot) == 0);

    assert(snapshot.percent_valid);
    assert(snapshot.percent == 73);

    assert(snapshot.status_valid);
    assert(snapshot.status ==
           FLIP_BATTERY_STATUS_DISCHARGING);

    assert(snapshot.voltage_valid);
    assert(snapshot.voltage_uv == 4012000);

    assert(snapshot.current_valid);
    assert(snapshot.current_ua == -321000);

    assert(snapshot.temp_valid);
    assert(snapshot.temp_deci_c == 245);

    assert(snapshot.capacity_level_valid);
    assert(snapshot.capacity_level == FLIP_BATTERY_CAPACITY_NORMAL);

    assert(snapshot.external_power_valid);
    assert(snapshot.external_power_online);
}

static void test_invalid_values_are_ignored(void)
{
    reset_root();

    write_value("capacity", "101\n");
    write_value("status", "Mystery\n");
    write_value("voltage_now", "-1\n");
    write_value("current_now", "50000000\n");
    write_value("temp", "5000\n");
    write_value("capacity_level", "Mystery\n");
    write_value_at(FLIP_AC_ROOT, "online", "2\n");
    write_value_at(FLIP_USB_ROOT, "online", "wat\n");

    flip_battery_snapshot_t snapshot;

    assert(flip_battery_read(&snapshot) == 0);
    assert(!snapshot.percent_valid);
    assert(!snapshot.status_valid);
    assert(!snapshot.voltage_valid);
    assert(!snapshot.current_valid);
    assert(!snapshot.temp_valid);
    assert(!snapshot.capacity_level_valid);
    assert(!snapshot.external_power_valid);
}

static void test_status_names(void)
{
    assert(strcmp(
        flip_battery_status_name(
            FLIP_BATTERY_STATUS_CHARGING),
        "CHARGING") == 0);

    assert(strcmp(
        flip_battery_status_name(
            FLIP_BATTERY_STATUS_DISCHARGING),
        "DISCHARGING") == 0);

    assert(strcmp(
        flip_battery_status_name(
            FLIP_BATTERY_STATUS_NOT_CHARGING),
        "NOT_CHARGING") == 0);

    assert(strcmp(
        flip_battery_status_name(
            FLIP_BATTERY_STATUS_FULL),
        "FULL") == 0);

    assert(strcmp(
        flip_battery_status_name(
            FLIP_BATTERY_STATUS_UNKNOWN),
        "UNKNOWN") == 0);
}

static void test_capacity_level_names(void)
{
    assert(strcmp(flip_battery_capacity_level_name(
        FLIP_BATTERY_CAPACITY_CRITICAL), "CRITICAL") == 0);
    assert(strcmp(flip_battery_capacity_level_name(
        FLIP_BATTERY_CAPACITY_LOW), "LOW") == 0);
    assert(strcmp(flip_battery_capacity_level_name(
        FLIP_BATTERY_CAPACITY_NORMAL), "NORMAL") == 0);
    assert(strcmp(flip_battery_capacity_level_name(
        FLIP_BATTERY_CAPACITY_HIGH), "HIGH") == 0);
    assert(strcmp(flip_battery_capacity_level_name(
        FLIP_BATTERY_CAPACITY_FULL), "FULL") == 0);
    assert(strcmp(flip_battery_capacity_level_name(
        FLIP_BATTERY_CAPACITY_UNKNOWN), "UNKNOWN") == 0);
}

int main(void)
{
    assert(flip_battery_read(NULL) < 0);

    test_missing_is_nonfatal();
    test_complete_snapshot();
    test_invalid_values_are_ignored();
    test_status_names();
    test_capacity_level_names();

    printf("flip_battery tests passed\n");
    return EXIT_SUCCESS;
}
