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

int flip_hotkeys_handle(
    flip_hotkeys_t *hotkeys,
    const flip_input_state_t *input,
    flip_hotkeys_result_t *result)
{
    if (!hotkeys ||
        !input ||
        !result)
        return -1;

    /*
     * Service delayed persistence for global settings.
     *
     * This is intentionally cheap when nothing is dirty
     * and avoids SD writes for every volume/brightness step.
     */
    if (flip_settings_tick() < 0) {
        fprintf(
            stderr,
            "HOTKEYS: settings flush failed\n");
    }

    memset(
        result,
        0,
        sizeof(*result));

    int menu_now =
        input->down[
            FLIP_BTN_MENU];

    int vol_up_now =
        input->down[
            FLIP_BTN_VOL_UP];

    int vol_down_now =
        input->down[
            FLIP_BTN_VOL_DOWN];

    /*
     * FlipOS global hardware controls.
     *
     *   VOL+        -> volume up
     *   VOL-        -> volume down
     *   MENU+VOL+   -> brightness up
     *   MENU+VOL-   -> brightness down
     *
     * These controls belong to FlipOS rather than
     * individual emulators.
     */

    if (vol_up_now &&
        !hotkeys->previous_vol_up) {

        if (menu_now) {

            int brightness =
                flip_settings_get_brightness();

            brightness += 20;

            if (brightness > 255)
                brightness = 255;

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

        } else {

            if (flip_audio_set_volume(
                    flip_audio_get_volume() + 5) == 0) {

                result->volume_changed = 1;

                flip_osd_show_volume(
                    flip_audio_get_volume());
            }
        }
    }

    if (vol_down_now &&
        !hotkeys->previous_vol_down) {

        if (menu_now) {

            int brightness =
                flip_settings_get_brightness();

            brightness -= 20;

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

        } else {

            if (flip_audio_set_volume(
                    flip_audio_get_volume() - 5) == 0) {

                result->volume_changed = 1;

                flip_osd_show_volume(
                    flip_audio_get_volume());
            }
        }
    }

    hotkeys->previous_vol_up =
        vol_up_now;

    hotkeys->previous_vol_down =
        vol_down_now;

    return 0;
}
