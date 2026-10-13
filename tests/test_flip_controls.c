#define _POSIX_C_SOURCE 200809L

#include "flip_controls.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

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

    assert(out.system.gameswitcher_requested == 1);

    update(
        &controls,
        &input,
        &out);

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

    /*
     * START+SELECT belongs to the game now. It is no longer a global
     * FlipOS exit chord; Quick Menu owns deliberate game-exit UX.
     */
    clear_input(
        &input);

    flip_controls_init(
        &controls);

    input.down[FLIP_BTN_START] = 1;
    input.down[FLIP_BTN_SELECT] = 1;

    update(
        &controls,
        &input,
        &out);

    assert(out.game.down[FLIP_GAME_START] == 1);
    assert(out.game.down[FLIP_GAME_SELECT] == 1);
    assert(out.system.exit_requested == 0);

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

    /* volume hold repeats after initial delay */
    struct timespec repeat_wait = { .tv_sec = 0, .tv_nsec = 500000000L };
    nanosleep(&repeat_wait, NULL);

    update(
        &controls,
        &input,
        &out);

    assert(out.system.volume_step == 1);

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

    puts(
        "flip_controls tests passed");

    return 0;
}
