#include "flip_controls.h"

#include <string.h>

static void copy_game_buttons(
    flip_game_input_t *game,
    const flip_input_state_t *input)
{
    static const flip_button_t physical[] = {
        FLIP_BTN_UP,
        FLIP_BTN_DOWN,
        FLIP_BTN_LEFT,
        FLIP_BTN_RIGHT,
        FLIP_BTN_A,
        FLIP_BTN_B,
        FLIP_BTN_X,
        FLIP_BTN_Y,
        FLIP_BTN_L1,
        FLIP_BTN_R1,
        FLIP_BTN_L2,
        FLIP_BTN_R2,
        FLIP_BTN_START,
        FLIP_BTN_SELECT,
        FLIP_BTN_L3,
        FLIP_BTN_R3
    };

    for (int i = 0;
         i < FLIP_GAME_BUTTON_COUNT;
         ++i) {

        game->down[i] =
            input->down[physical[i]];
    }
}

void flip_controls_init(
    flip_controls_t *controls)
{
    if (!controls)
        return;

    memset(
        controls,
        0,
        sizeof(*controls));
}

int flip_controls_update(
    flip_controls_t *controls,
    const flip_input_state_t *input,
    flip_controls_state_t *state)
{
    if (!controls ||
        !input ||
        !state)
        return -1;

    memset(
        state,
        0,
        sizeof(*state));

    copy_game_buttons(
        &state->game,
        input);

    state->game.analog_ready =
        input->analog_ready;

    if (input->analog_ready) {
        state->game.lx =
            input->raw_lx -
            input->centre_lx;

        state->game.ly =
            input->raw_ly -
            input->centre_ly;

        state->game.rx =
            input->raw_rx -
            input->centre_rx;

        state->game.ry =
            input->raw_ry -
            input->centre_ry;
    }

    int menu_now =
        input->down[FLIP_BTN_MENU];

    int r1_now =
        input->down[FLIP_BTN_R1];

    int start_now =
        input->down[FLIP_BTN_START];

    int select_now =
        input->down[FLIP_BTN_SELECT];

    int vol_up_now =
        input->down[FLIP_BTN_VOL_UP];

    int vol_down_now =
        input->down[FLIP_BTN_VOL_DOWN];

    if (menu_now &&
        !controls->previous_menu) {

        controls->menu_pending = 1;
    }

    if (menu_now &&
        r1_now &&
        !controls->previous_r1) {

        state->system.fps_toggle_requested = 1;
        controls->menu_pending = 0;
    }

    if (vol_up_now &&
        !controls->previous_vol_up) {

        if (menu_now) {
            state->system.brightness_step = 1;
            controls->menu_pending = 0;
        } else {
            state->system.volume_step = 1;
        }
    }

    if (vol_down_now &&
        !controls->previous_vol_down) {

        if (menu_now) {
            state->system.brightness_step = -1;
            controls->menu_pending = 0;
        } else {
            state->system.volume_step = -1;
        }
    }

    int exit_now =
        start_now &&
        select_now;

    int exit_before =
        controls->previous_start &&
        controls->previous_select;

    if (exit_now &&
        !exit_before) {

        state->system.exit_requested = 1;
    }

    state->system.lid_ready =
        input->lid_ready;

    state->system.lid_closed =
        input->lid_closed;

    if (input->lid_ready &&
        (!controls->previous_lid_ready ||
         input->lid_closed !=
             controls->previous_lid_closed)) {

        state->system.lid_changed = 1;

        if (controls->previous_lid_ready &&
            input->lid_closed) {

            state->system.suspend_requested = 1;
        }
    }

    if (!menu_now &&
        controls->previous_menu &&
        controls->menu_pending) {

        state->system.gameswitcher_requested = 1;
        controls->menu_pending = 0;
    }

    if (!menu_now &&
        controls->previous_menu) {

        controls->menu_pending = 0;
    }

    controls->previous_menu =
        menu_now;

    controls->previous_r1 =
        r1_now;

    controls->previous_start =
        start_now;

    controls->previous_select =
        select_now;

    controls->previous_vol_up =
        vol_up_now;

    controls->previous_vol_down =
        vol_down_now;

    controls->previous_lid_ready =
        input->lid_ready;

    controls->previous_lid_closed =
        input->lid_closed;

    return 0;
}
