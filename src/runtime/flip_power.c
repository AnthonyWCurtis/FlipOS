#include "flip_power.h"

#include <stdio.h>
#include <unistd.h>

#ifndef FLIP_BACKLIGHT_BRIGHTNESS
#define FLIP_BACKLIGHT_BRIGHTNESS \
    "/sys/class/backlight/backlight/brightness"
#endif

#ifndef FLIP_POWER_MEM_SLEEP
#define FLIP_POWER_MEM_SLEEP \
    "/sys/power/mem_sleep"
#endif

#ifndef FLIP_POWER_STATE
#define FLIP_POWER_STATE \
    "/sys/power/state"
#endif

#define FLIP_DEV_LID_OVERRIDE "/tmp/flipos-dev-lid-override"
#define FLIP_DEV_LID_AUTO_OPEN "/tmp/flipos-dev-lid-auto-open-after-resume"
#define FLIP_BRIGHTNESS_MIN 10
#define FLIP_BRIGHTNESS_MAX 255

static int read_backlight_brightness(void)
{
    FILE *f =
        fopen(
            FLIP_BACKLIGHT_BRIGHTNESS,
            "r");

    if (!f)
        return -1;

    int brightness = -1;

    if (fscanf(
            f,
            "%d",
            &brightness) != 1) {

        brightness = -1;
    }

    fclose(f);

    return brightness;
}

static int write_value(
    const char *path,
    const char *value)
{
    FILE *f =
        fopen(
            path,
            "w");

    if (!f)
        return -1;

    int ok =
        fprintf(
            f,
            "%s\n",
            value);

    if (fflush(f) != 0)
        ok = -1;

    fclose(f);

    return ok < 0
        ? -1
        : 0;
}

static int write_backlight_brightness(
    int brightness)
{
    char value[32];

    snprintf(
        value,
        sizeof(value),
        "%d",
        brightness);

    return write_value(
        FLIP_BACKLIGHT_BRIGHTNESS,
        value);
}

flip_power_lid_state_t flip_power_refresh_lid_state(
    flip_input_state_t *state)
{
    if (!state)
        return FLIP_POWER_LID_ERROR;

    if (flip_input_poll(
            state,
            0) < 0)
        return FLIP_POWER_LID_ERROR;

    if (!state->lid_ready)
        return FLIP_POWER_LID_UNKNOWN;

    return state->lid_closed
        ? FLIP_POWER_LID_CLOSED
        : FLIP_POWER_LID_OPEN;
}

static void dev_lid_auto_open_after_resume(void)
{
    if (access(FLIP_DEV_LID_AUTO_OPEN, F_OK) != 0)
        return;

    FILE *f = fopen(FLIP_DEV_LID_OVERRIDE, "w");
    if (f) {
        if (fputs("open\n", f) >= 0)
            (void)fflush(f);
        (void)fclose(f);
    }
    (void)unlink(FLIP_DEV_LID_AUTO_OPEN);
}

int flip_power_sleep_once(void)
{
    int saved_brightness =
        read_backlight_brightness();

    if (saved_brightness < 0)
        saved_brightness = 200;

    fprintf(
        stderr,
        "POWER: menu sleep enter brightness=%d\n",
        saved_brightness);

    if (write_backlight_brightness(0) < 0) {
        fprintf(
            stderr,
            "POWER: menu backlight off failed\n");
        return -1;
    }

    if (write_value(
            FLIP_POWER_MEM_SLEEP,
            "deep") < 0) {

        fprintf(
            stderr,
            "POWER: menu select deep failed\n");

        (void)write_backlight_brightness(
            saved_brightness);

        return -1;
    }

    /*
     * Flush SD-backed state before userspace is frozen. The kernel resumes
     * execution immediately after this write when a configured wake source
     * (POWER, lid, charger, etc.) fires.
     */
    sync();

    if (write_value(
            FLIP_POWER_STATE,
            "mem") < 0) {

        fprintf(
            stderr,
            "POWER: menu suspend failed\n");

        (void)write_backlight_brightness(
            saved_brightness);

        return -1;
    }

    if (write_backlight_brightness(
            saved_brightness) < 0) {

        fprintf(
            stderr,
            "POWER: menu backlight restore failed\n");

        return -1;
    }

    fprintf(
        stderr,
        "POWER: menu sleep exit brightness=%d\n",
        saved_brightness);

    return 0;
}

int flip_power_sleep_until_lid_open(
    flip_input_state_t *state)
{
    if (!state)
        return -1;

    int saved_brightness =
        read_backlight_brightness();

    if (saved_brightness < 0)
        saved_brightness = 200;

    fprintf(
        stderr,
        "POWER: deep sleep enter brightness=%d\n",
        saved_brightness);

    if (write_backlight_brightness(
            0) < 0) {

        fprintf(
            stderr,
            "POWER: backlight off failed\n");

        return -1;
    }

    if (write_value(
            FLIP_POWER_MEM_SLEEP,
            "deep") < 0) {

        fprintf(
            stderr,
            "POWER: select deep failed\n");

        write_backlight_brightness(
            saved_brightness);

        return -1;
    }

    for (;;) {

        if (flip_input_poll(
                state,
                0) < 0) {

            fprintf(
                stderr,
                "POWER: lid poll failed\n");

            write_backlight_brightness(
                saved_brightness);

            return -1;
        }

        /*
         * Lid may have reopened before suspend actually
         * started. In that case there is no reason to sleep.
         */
        if (state->lid_ready &&
            !state->lid_closed) {

            fprintf(
                stderr,
                "POWER: lid already open\n");

            break;
        }

        fprintf(
            stderr,
            "POWER: suspending\n");

        /*
         * Preserve SD-backed session/state data before
         * userspace is frozen.
         */
        sync();

        if (write_value(
                FLIP_POWER_STATE,
                "mem") < 0) {

            fprintf(
                stderr,
                "POWER: suspend failed\n");

            write_backlight_brightness(
                saved_brightness);

            return -1;
        }

        /*
         * Execution continues here after a wake interrupt.
         */
        fprintf(
            stderr,
            "POWER: resumed\n");

        /* Development automation can model a physical lid opening at wake
         * while still exercising the real kernel suspend/resume path. */
        dev_lid_auto_open_after_resume();

        if (flip_input_poll(
                state,
                0) < 0) {

            fprintf(
                stderr,
                "POWER: post-wake lid poll failed\n");

            write_backlight_brightness(
                saved_brightness);

            return -1;
        }

        if (state->lid_ready &&
            !state->lid_closed) {

            fprintf(
                stderr,
                "POWER: lid open\n");

            break;
        }

        /*
         * A charger, POWER key, RTC, etc. may wake the
         * machine while the lid is still closed.
         *
         * In that case return directly to deep suspend.
         */
        fprintf(
            stderr,
            "POWER: wake while closed; resleep\n");
    }

    if (write_backlight_brightness(
            saved_brightness) < 0) {

        fprintf(
            stderr,
            "POWER: backlight restore failed\n");

        return -1;
    }

    fprintf(
        stderr,
        "POWER: deep sleep exit brightness=%d\n",
        saved_brightness);

    return 0;
}

int flip_power_get_brightness(void)
{
    return read_backlight_brightness();
}

int flip_power_brightness_to_percent(int brightness)
{
    if (brightness <= FLIP_BRIGHTNESS_MIN)
        return 0;
    if (brightness >= FLIP_BRIGHTNESS_MAX)
        return 100;

    return
        ((brightness - FLIP_BRIGHTNESS_MIN) * 100 +
         (FLIP_BRIGHTNESS_MAX - FLIP_BRIGHTNESS_MIN) / 2) /
        (FLIP_BRIGHTNESS_MAX - FLIP_BRIGHTNESS_MIN);
}

int flip_power_percent_to_brightness(int percent)
{
    if (percent <= 0)
        return FLIP_BRIGHTNESS_MIN;
    if (percent >= 100)
        return FLIP_BRIGHTNESS_MAX;

    return
        FLIP_BRIGHTNESS_MIN +
        (percent * (FLIP_BRIGHTNESS_MAX - FLIP_BRIGHTNESS_MIN) + 50) /
            100;
}

int flip_power_set_brightness(int brightness)
{
    if (brightness < FLIP_BRIGHTNESS_MIN)
        brightness = FLIP_BRIGHTNESS_MIN;

    if (brightness > FLIP_BRIGHTNESS_MAX)
        brightness = FLIP_BRIGHTNESS_MAX;

    if (write_backlight_brightness(brightness) < 0)
        return -1;

    fprintf(
        stderr,
        "POWER: brightness=%d percent=%d\n",
        brightness,
        flip_power_brightness_to_percent(brightness));

    return 0;
}
