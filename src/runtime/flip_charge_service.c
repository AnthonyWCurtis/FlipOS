#define _POSIX_C_SOURCE 200809L

#include "flip_battery.h"
#include "flip_charger.h"
#include "flip_log.h"
#include "flip_paths.h"

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t running = 1;

static void handle_signal(int signal_number)
{
    (void)signal_number;
    running = 0;
}

int main(void)
{
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    FILE *service_log = NULL;
    char log_path[512];
    (void)mkdir(flip_paths_log_root(), 0755);
    if (snprintf(
            log_path, sizeof(log_path), "%s/charge.log",
            flip_paths_log_root()) > 0) {
        service_log = fopen(log_path, "a");
        if (service_log)
            (void)flip_log_set_stream(service_log);
    }

    flip_charger_t charger;
    flip_charger_init(&charger);

    if (!charger.helper_available) {
        flip_log_write(
            FLIP_LOG_WARN,
            "FLIP_CHARGE",
            "RK817 charge-current helper unavailable; stock charging retained");
        if (service_log)
            fclose(service_log);
        return 0;
    }

    flip_log_write(
        FLIP_LOG_INFO,
        "FLIP_CHARGE",
        "service started helper=1 boot_current_ma=%d",
        charger.stock_current_ma);

    flip_charge_mode_t last_mode = (flip_charge_mode_t)-1;
    int last_applied_ma = -999;

    while (running) {
        flip_battery_snapshot_t battery;
        if (flip_battery_read(&battery) == 0) {
            int update_result = flip_charger_update(&charger, &battery);
            if (update_result < 0) {
                flip_log_write(
                    FLIP_LOG_ERROR,
                    "FLIP_CHARGE",
                    "charge policy apply failed; attempting stock release");
                (void)flip_charger_restore(&charger);
            } else if (charger.mode != last_mode ||
                       charger.applied_current_ma != last_applied_ma) {
                flip_log_write(
                    FLIP_LOG_INFO,
                    "FLIP_CHARGE",
                    "mode=%s temp_deci_c=%d external=%d cap_ma=%d",
                    flip_charge_mode_name(charger.mode),
                    battery.temp_valid ? battery.temp_deci_c : -9999,
                    battery.external_power_valid
                        ? battery.external_power_online
                        : -1,
                    charger.applied_current_ma);
                last_mode = charger.mode;
                last_applied_ma = charger.applied_current_ma;
            }
        }

        struct timespec delay = {
            .tv_sec = 1,
            .tv_nsec = 0,
        };
        while (running && nanosleep(&delay, &delay) < 0)
            ;
    }

    if (flip_charger_restore(&charger) < 0) {
        flip_log_write(
            FLIP_LOG_ERROR,
            "FLIP_CHARGE",
            "service stop failed to release charge-current cap");
        return 1;
    }

    flip_log_write(
        FLIP_LOG_INFO,
        "FLIP_CHARGE",
        "service stopped; stock charger control released");
    if (service_log)
        fclose(service_log);
    return 0;
}
