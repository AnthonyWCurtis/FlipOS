#include "flip_controls.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void clear_input(
    flip_input_state_t *input)
{
    memset(
        input,
        0,
        sizeof(*input));
}

static void update(
    flip_controls_t *controls,
    flip_input_state_t *input,
    flip_controls_state_t *out)
{
    assert(
        flip_controls_update(
            controls,
            input,
            out) == 0);
}

int main(void)
{
    flip_controls_t controls;
    flip_controls_state_t out;
    flip_input_state_t input;

    flip_controls_init(
        &controls);

    clear_input(
        &input);

    input.down[FLIP_BTN_UP] = 1;
    input.down[FLIP_BTN_A] = 1;
    input.down[FLIP_BTN_START] = 1;
    input.analog_ready = 1;
    input.raw_lx = 140;
    input.centre_lx = 128;
    input.raw_ly = 120;
    input.centre_ly = 127;

    update(
        &controls,
        &input,
        &out);

    assert(out.game.down[FLIP_GAME_UP] == 1);
    assert(out.game.down[FLIP_GAME_A] == 1);
    assert(out.game.down[FLIP_GAME_START] == 1);
    assert(out.game.lx == 12);
    assert(out.game.ly == -7);

    clear_input(
        &input);

    flip_controls_init(
        &controls);

    input.down[FLIP_BTN_MENU] = 1;

    update(
        &controls,
        &input,
        &out);

    assert(out.system.gameswitcher_requested == 0);

    clear_input(
        &input);

    update(
        &controls,
        &input,
        &out);

    assert(out.system.game_options_requested == 1);
    assert(out.system.gameswitcher_requested == 0);

    update(
        &controls,
        &input,
        &out);

    assert(out.system.game_options_requested == 0);
    assert(out.system.gameswitcher_requested == 0);

    clear_input(
        &input);

    flip_controls_init(
        &controls);

    input.down[FLIP_BTN_MENU] = 1;

    update(
        &controls,
        &input,
        &out);

    input.down[FLIP_BTN_R1] = 1;

    update(
        &controls,
        &input,
        &out);

    assert(out.system.fps_toggle_requested == 1);

    clear_input(
        &input);

    update(
        &controls,
        &input,
        &out);

    assert(out.system.gameswitcher_requested == 0);

    clear_input(
        &input);

    flip_controls_init(
        &controls);

    /*
     * START + SELECT are normal game inputs now.
     * They must no longer request Exit Game.
     */
    input.down[FLIP_BTN_START] = 1;
    input.down[FLIP_BTN_SELECT] = 1;

    update(
        &controls,
        &input,
        &out);

    assert(out.system.exit_requested == 0);
    assert(out.game.down[FLIP_GAME_START] == 1);
    assert(out.game.down[FLIP_GAME_SELECT] == 1);

    /*
     * MENU + START is the sole Exit Game gesture.
     */
    clear_input(
        &input);

    flip_controls_init(
        &controls);

    input.down[FLIP_BTN_MENU] = 1;
    input.down[FLIP_BTN_START] = 1;

    update(
        &controls,
        &input,
        &out);

    assert(out.system.exit_requested == 1);
    assert(out.system.gameswitcher_requested == 0);

    /*
     * Holding the combo must not retrigger Exit Game.
     */
    update(
        &controls,
        &input,
        &out);

    assert(out.system.exit_requested == 0);

    /*
     * Releasing MENU after the exit chord must not emit a
     * delayed GameSwitcher request.
     */
    input.down[FLIP_BTN_MENU] = 0;

    update(
        &controls,
        &input,
        &out);

    assert(out.system.gameswitcher_requested == 0);

    clear_input(
        &input);

    flip_controls_init(
        &controls);

    input.down[FLIP_BTN_VOL_UP] = 1;

    update(
        &controls,
        &input,
        &out);

    assert(out.system.volume_step == 1);

    update(
        &controls,
        &input,
        &out);

    assert(out.system.volume_step == 0);

    clear_input(
        &input);

    flip_controls_init(
        &controls);

    input.down[FLIP_BTN_MENU] = 1;

    update(
        &controls,
        &input,
        &out);

    input.down[FLIP_BTN_VOL_DOWN] = 1;

    update(
        &controls,
        &input,
        &out);

    assert(out.system.brightness_step == -1);

    clear_input(
        &input);

    update(
        &controls,
        &input,
        &out);

    assert(out.system.gameswitcher_requested == 0);

    clear_input(
        &input);

    flip_controls_init(
        &controls);

    input.lid_ready = 1;
    input.lid_closed = 0;

    update(
        &controls,
        &input,
        &out);

    assert(out.system.lid_changed == 1);
    assert(out.system.suspend_requested == 0);

    input.lid_closed = 1;

    update(
        &controls,
        &input,
        &out);

    assert(out.system.lid_changed == 1);
    assert(out.system.suspend_requested == 1);

    update(
        &controls,
        &input,
        &out);

    assert(out.system.suspend_requested == 0);


    /*
     * MENU tap opens Game Options.
     */
    clear_input(
        &input);

    flip_controls_init(
        &controls);

    input.down[FLIP_BTN_MENU] = 1;

    assert(flip_controls_update_at(
        &controls,
        &input,
        &out,
        1000) == 0);

    assert(out.system.game_options_requested == 0);
    assert(out.system.gameswitcher_requested == 0);

    input.down[FLIP_BTN_MENU] = 0;

    assert(flip_controls_update_at(
        &controls,
        &input,
        &out,
        1200) == 0);

    assert(out.system.game_options_requested == 1);
    assert(out.system.gameswitcher_requested == 0);

    /*
     * MENU hold opens GameSwitcher once the 500 ms
     * threshold is reached, without waiting for release.
     */
    clear_input(
        &input);

    flip_controls_init(
        &controls);

    input.down[FLIP_BTN_MENU] = 1;

    assert(flip_controls_update_at(
        &controls,
        &input,
        &out,
        2000) == 0);

    assert(out.system.game_options_requested == 0);
    assert(out.system.gameswitcher_requested == 0);

    assert(flip_controls_update_at(
        &controls,
        &input,
        &out,
        2499) == 0);

    assert(out.system.game_options_requested == 0);
    assert(out.system.gameswitcher_requested == 0);

    assert(flip_controls_update_at(
        &controls,
        &input,
        &out,
        2500) == 0);

    assert(out.system.game_options_requested == 0);
    assert(out.system.gameswitcher_requested == 1);

    /*
     * Continuing to hold MENU must not repeatedly request
     * GameSwitcher.
     */
    assert(flip_controls_update_at(
        &controls,
        &input,
        &out,
        2700) == 0);

    assert(out.system.game_options_requested == 0);
    assert(out.system.gameswitcher_requested == 0);

    /*
     * Releasing MENU after the hold has already emitted
     * GameSwitcher must NOT emit delayed Game Options.
     */
    input.down[FLIP_BTN_MENU] = 0;

    assert(flip_controls_update_at(
        &controls,
        &input,
        &out,
        2800) == 0);

    assert(out.system.game_options_requested == 0);
    assert(out.system.gameswitcher_requested == 0);

    puts(
        "flip_controls tests passed");

    return 0;
}
