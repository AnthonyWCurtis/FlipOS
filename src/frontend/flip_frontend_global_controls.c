#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "flip_frontend_global_controls.h"

#include "flip_osd.h"

#include <stdio.h>
#include <string.h>
#include <sys/reboot.h>
#include <time.h>
#include <unistd.h>

#define POWEROFF_HOLD_SECONDS 1.5

static double monotonic_seconds(void)
{
    struct timespec ts;

    if (clock_gettime(
            CLOCK_MONOTONIC,
            &ts) < 0)
        return 0.0;

    return
        (double)ts.tv_sec +
        (double)ts.tv_nsec /
            1000000000.0;
}

void flip_frontend_global_controls_init(
    flip_frontend_global_controls_t *controls)
{
    if (!controls)
        return;

    memset(
        controls,
        0,
        sizeof(*controls));

    flip_hotkeys_init(
        &controls->hotkeys);
}

int flip_frontend_global_controls_handle(
    flip_frontend_global_controls_t *controls,
    const flip_input_state_t *input,
    flip_frontend_global_controls_result_t *result)
{
    if (!controls ||
        !input ||
        !result)
        return -1;

    memset(
        result,
        0,
        sizeof(*result));

    flip_hotkeys_result_t hotkey_result;

    if (flip_hotkeys_handle(
            &controls->hotkeys,
            input,
            &hotkey_result) < 0) {

        fprintf(
            stderr,
            "GLOBAL_CONTROLS: hotkey handling failed\n");

        return -1;
    }

    if (hotkey_result.volume_changed ||
        hotkey_result.brightness_changed) {

        result->redraw = 1;
    }

    if (input->down[FLIP_BTN_MENU] &&
        input->down[FLIP_BTN_SELECT]) {

        double now =
            monotonic_seconds();

        if (!controls->poweroff_active) {
            controls->poweroff_active = 1;
            controls->poweroff_started = now;

            fprintf(
                stderr,
                "GLOBAL_CONTROLS: poweroff hold started\n");
        }

        double held =
            now -
            controls->poweroff_started;

        double progress =
            held /
            POWEROFF_HOLD_SECONDS;

        if (progress < 0.0)
            progress = 0.0;

        if (progress > 1.0)
            progress = 1.0;

        result->poweroff_active = 1;
        result->poweroff_progress =
            progress;
        result->redraw = 1;

        flip_osd_set_poweroff_progress(
            progress);

        if (held >=
            POWEROFF_HOLD_SECONDS) {

            result->shutdown_requested = 1;

            fprintf(
                stderr,
                "GLOBAL_CONTROLS: poweroff requested\n");

            fflush(stderr);
            sync();

            reboot(
                RB_POWER_OFF);

            perror(
                "GLOBAL_CONTROLS: reboot(RB_POWER_OFF)");
        }

    } else {

        if (controls->poweroff_active) {
            result->redraw = 1;

            fprintf(
                stderr,
                "GLOBAL_CONTROLS: poweroff hold cancelled\n");
        }

        controls->poweroff_active = 0;

        flip_osd_set_poweroff_progress(
            0.0);
    }

    return 0;
}
