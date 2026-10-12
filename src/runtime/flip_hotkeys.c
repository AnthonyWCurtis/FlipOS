#include "flip_hotkeys.h"

#include "flip_audio.h"
#include "flip_osd.h"
#include "flip_power.h"
#include "flip_settings.h"

#include <stdio.h>
#include <string.h>

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
