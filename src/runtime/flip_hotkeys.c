#define _POSIX_C_SOURCE 200809L

#include "flip_hotkeys.h"

#include "flip_audio.h"
#include "flip_osd.h"
#include "flip_power.h"
#include "flip_settings.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#define FLIP_HOTKEY_REPEAT_INITIAL_MS 450ULL
#define FLIP_HOTKEY_REPEAT_INTERVAL_MS 120ULL

static uint64_t monotonic_ms(void)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0)
        return 0;
    return (uint64_t)now.tv_sec * 1000ULL + (uint64_t)now.tv_nsec / 1000000ULL;
}

static int held_repeat_due(int down, int previous, uint64_t *repeat_at_ms, uint64_t now_ms)
{
    if (!down) {
        *repeat_at_ms = 0;
        return 0;
    }

    if (!previous) {
        *repeat_at_ms = now_ms + FLIP_HOTKEY_REPEAT_INITIAL_MS;
        return 1;
    }

    if (*repeat_at_ms != 0 && now_ms >= *repeat_at_ms) {
        *repeat_at_ms = now_ms + FLIP_HOTKEY_REPEAT_INTERVAL_MS;
        return 1;
    }

    return 0;
}

void flip_hotkeys_init(
    flip_hotkeys_t *hotkeys)
{
    if (!hotkeys)
        return;

    memset(
        hotkeys,
        0,
        sizeof(*hotkeys));
}

int flip_hotkeys_apply_actions(
    const flip_system_actions_t *actions,
    flip_hotkeys_result_t *result)
{
    if (!actions ||
        !result)
        return -1;

    if (flip_settings_tick() < 0) {
        fprintf(
            stderr,
            "HOTKEYS: settings flush failed\n");
    }

    memset(
        result,
        0,
        sizeof(*result));

    if (actions->volume_step != 0) {

        int volume =
            flip_audio_get_volume() +
            actions->volume_step * 5;

        if (flip_audio_set_volume(
                volume) == 0) {

            result->volume_changed = 1;

            flip_osd_show_volume(
                flip_audio_get_volume());
        }
    }

    if (actions->brightness_step != 0) {

        int current_percent =
            flip_power_brightness_to_percent(
                flip_settings_get_brightness());

        int brightness_percent;

        if (actions->brightness_step > 0) {
            brightness_percent =
                ((current_percent / 10) + 1) * 10;
            if (brightness_percent > 100)
                brightness_percent = 100;
        } else {
            brightness_percent =
                current_percent > 0
                    ? ((current_percent - 1) / 10) * 10
                    : 0;
        }

        int brightness =
            flip_power_percent_to_brightness(
                brightness_percent);

        if (flip_power_set_brightness(
                brightness) == 0) {

            if (flip_settings_set_brightness(
                    brightness) < 0) {

                fprintf(
                    stderr,
                    "HOTKEYS: brightness persistence failed\n");

            } else {

                result->brightness_changed = 1;
            }

            result->menu_consumed = 1;

            flip_osd_show_brightness(
                brightness);

            fprintf(
                stderr,
                "HOTKEYS: brightness=%d\n",
                brightness);
        }
    }

    return 0;
}

int flip_hotkeys_handle(
    flip_hotkeys_t *hotkeys,
    const flip_input_state_t *input,
    flip_hotkeys_result_t *result)
{
    if (!hotkeys ||
        !input ||
        !result)
        return -1;

    flip_system_actions_t actions;

    memset(
        &actions,
        0,
        sizeof(actions));

    int menu_now =
        input->down[
            FLIP_BTN_MENU];

    int vol_up_now =
        input->down[
            FLIP_BTN_VOL_UP];

    int vol_down_now =
        input->down[
            FLIP_BTN_VOL_DOWN];

    uint64_t now_ms = monotonic_ms();

    int vol_up_step =
        held_repeat_due(
            vol_up_now,
            hotkeys->previous_vol_up,
            &hotkeys->vol_up_repeat_at_ms,
            now_ms);

    int vol_down_step =
        held_repeat_due(
            vol_down_now,
            hotkeys->previous_vol_down,
            &hotkeys->vol_down_repeat_at_ms,
            now_ms);

    if (vol_up_step && !vol_down_now) {
        if (menu_now)
            actions.brightness_step += 1;
        else
            actions.volume_step += 1;
    }

    if (vol_down_step && !vol_up_now) {
        if (menu_now)
            actions.brightness_step -= 1;
        else
            actions.volume_step -= 1;
    }

    hotkeys->previous_vol_up =
        vol_up_now;

    hotkeys->previous_vol_down =
        vol_down_now;

    return flip_hotkeys_apply_actions(
        &actions,
        result);
}
