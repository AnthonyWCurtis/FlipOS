#include "flip_hotkeys.h"

#include "flip_audio.h"
#include "flip_osd.h"
#include "flip_power.h"
#include "flip_settings.h"

#include <stdio.h>
#include <string.h>

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

        int brightness =
            flip_settings_get_brightness() +
            actions->brightness_step * 20;

        if (brightness > 255)
            brightness = 255;

        if (brightness < 10)
            brightness = 10;

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

    if (vol_up_now &&
        !hotkeys->previous_vol_up) {

        if (menu_now) {
            actions.brightness_step += 1;
        } else {
            actions.volume_step += 1;
        }
    }

    if (vol_down_now &&
        !hotkeys->previous_vol_down) {

        if (menu_now) {
            actions.brightness_step -= 1;
        } else {
            actions.volume_step -= 1;
        }
    }

    hotkeys->previous_vol_up =
        vol_up_now;

    hotkeys->previous_vol_down =
        vol_down_now;

    return flip_hotkeys_apply_actions(
        &actions,
        result);
}
