#include "flip_power.h"

#include <stdio.h>
#include <unistd.h>

#define FLIP_BACKLIGHT_BRIGHTNESS \
    "/sys/class/backlight/backlight/brightness"

#define FLIP_POWER_MEM_SLEEP \
    "/sys/power/mem_sleep"

#define FLIP_POWER_STATE \
    "/sys/power/state"

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
    FILE *f =
        fopen(
            "/sys/class/backlight/backlight/brightness",
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

int flip_power_set_brightness(
    int brightness)
{
    if (brightness < 10)
        brightness = 10;

    if (brightness > 255)
        brightness = 255;

    FILE *f =
        fopen(
            "/sys/class/backlight/backlight/brightness",
            "w");

    if (!f)
        return -1;

    int ok =
        fprintf(
            f,
            "%d\n",
            brightness);

    fclose(f);

    if (ok < 0)
        return -1;

    fprintf(
        stderr,
        "POWER: brightness=%d\n",
        brightness);

    return 0;
}
