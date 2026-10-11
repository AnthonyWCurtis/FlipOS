#include "flip_charger.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void write_file(const char *path, const char *value)
{
    FILE *f = fopen(path, "w");
    assert(f != NULL);
    assert(fputs(value, f) >= 0);
    fclose(f);
}

static int read_current(void)
{
    FILE *f = fopen(FLIP_CHARGER_CURRENT_LIMIT_PATH, "r");
    assert(f != NULL);
    int value = -99;
    assert(fscanf(f, "%d", &value) == 1);
    fclose(f);
    return value;
}

static flip_battery_snapshot_t charging_battery(int temp)
{
    flip_battery_snapshot_t battery;
    memset(&battery, 0, sizeof(battery));
    battery.external_power_valid = 1;
    battery.external_power_online = 1;
    battery.temp_valid = 1;
    battery.temp_deci_c = temp;
    return battery;
}

int main(void)
{
    assert(system("rm -rf /tmp/flipos-charger-test && mkdir -p /tmp/flipos-charger-test") == 0);
    write_file(FLIP_CHARGER_CURRENT_LIMIT_PATH, "-1\n");
    write_file(FLIP_CHARGER_STOCK_CURRENT_PATH, "2500\n");

    flip_charger_t charger;
    flip_charger_init(&charger);
    assert(charger.helper_available);
    assert(charger.stock_current_ma == 2500);

    flip_battery_snapshot_t battery = charging_battery(250);
    assert(flip_charger_update(&charger, &battery) == 1);
    assert(read_current() == -1);

    battery.temp_deci_c = 410;
    assert(flip_charger_update(&charger, &battery) == 1);
    assert(read_current() == 1500);

    battery.temp_deci_c = 460;
    assert(flip_charger_update(&charger, &battery) == 1);
    assert(read_current() == 1000);

    battery.temp_deci_c = 510;
    assert(flip_charger_update(&charger, &battery) == 1);
    assert(read_current() == 500);

    battery.external_power_online = 0;
    assert(flip_charger_update(&charger, &battery) == 1);
    assert(read_current() == -1);

    assert(flip_charger_restore(&charger) == 0);
    assert(read_current() == -1);

    puts("flip_charger tests passed");
    return EXIT_SUCCESS;
}
