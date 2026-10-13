#define _POSIX_C_SOURCE 200809L

#include "flip_controls.h"

#include <string.h>
#include <time.h>

#define FLIP_HOTKEY_REPEAT_INITIAL_MS 450ULL
#define FLIP_HOTKEY_REPEAT_INTERVAL_MS 120ULL
#define FLIP_MENU_GAMESWITCHER_HOLD_MS 700ULL
#define FLIP_POWER_MENU_TAP_MAX_MS 700ULL

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

    int power_now =
        input->down[FLIP_BTN_POWER];

    int r1_now =
        input->down[FLIP_BTN_R1];

    int select_now =
        input->down[FLIP_BTN_SELECT];

    int vol_up_now =
        input->down[FLIP_BTN_VOL_UP];

    int vol_down_now =
        input->down[FLIP_BTN_VOL_DOWN];

    uint64_t now_ms = monotonic_ms();

    if (menu_now &&
        !controls->previous_menu) {

        controls->menu_pending = 1;
        controls->menu_pressed_at_ms = now_ms;
    }

    /*
     * POWER deliberately triggers on release, not press. A short press opens
     * the software Power Menu; holding beyond the tap window is left entirely
     * to the device's hardware/emergency power path.
     */
    if (power_now &&
        !controls->previous_power) {

        controls->power_pending = 1;
        controls->power_pressed_at_ms = now_ms;
    }

    if (power_now &&
        controls->power_pending &&
        now_ms >= controls->power_pressed_at_ms &&
        now_ms - controls->power_pressed_at_ms >
            FLIP_POWER_MENU_TAP_MAX_MS) {

        controls->power_pending = 0;
    }

    if (!power_now &&
        controls->previous_power &&
        controls->power_pending) {

        uint64_t held_ms =
            now_ms >= controls->power_pressed_at_ms
                ? now_ms - controls->power_pressed_at_ms
                : 0;

        if (held_ms <=
            FLIP_POWER_MENU_TAP_MAX_MS) {

            state->system.power_menu_requested = 1;
        }

        controls->power_pending = 0;
        controls->power_pressed_at_ms = 0;
    }

    /*
     * MENU+SELECT is deliberately not a FlipOS chord. SELECT remains a game
     * button and consuming the pending MENU action prevents either Quick Menu
     * or GameSwitcher from firing when the two are pressed together.
     */
    if (menu_now &&
        select_now) {

        controls->menu_pending = 0;
    }

    if (menu_now &&
        r1_now &&
        !controls->previous_r1) {

        state->system.fps_toggle_requested = 1;
        controls->menu_pending = 0;
    }

    int vol_up_step =
        held_repeat_due(
            vol_up_now,
            controls->previous_vol_up,
            &controls->vol_up_repeat_at_ms,
            now_ms);

    int vol_down_step =
        held_repeat_due(
            vol_down_now,
            controls->previous_vol_down,
            &controls->vol_down_repeat_at_ms,
            now_ms);

    if (vol_up_step && !vol_down_now) {
        if (menu_now) {
            state->system.brightness_step = 1;
            controls->menu_pending = 0;
        } else {
            state->system.volume_step = 1;
        }
    }

    if (vol_down_step && !vol_up_now) {
        if (menu_now) {
            state->system.brightness_step = -1;
            controls->menu_pending = 0;
        } else {
            state->system.volume_step = -1;
        }
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

    /*
     * MENU is the single in-game FlipOS key:
     *   tap         -> Quick Menu
     *   hold 700 ms -> GameSwitcher
     */
    if (menu_now &&
        controls->menu_pending &&
        now_ms >= controls->menu_pressed_at_ms &&
        now_ms - controls->menu_pressed_at_ms >=
            FLIP_MENU_GAMESWITCHER_HOLD_MS) {

        state->system.gameswitcher_requested = 1;
        controls->menu_pending = 0;
    }

    if (!menu_now &&
        controls->previous_menu &&
        controls->menu_pending) {

        state->system.quick_menu_requested = 1;
        controls->menu_pending = 0;
    }

    if (!menu_now &&
        controls->previous_menu) {

        controls->menu_pending = 0;
        controls->menu_pressed_at_ms = 0;
    }

    controls->previous_menu =
        menu_now;

    controls->previous_power =
        power_now;

    controls->previous_r1 =
        r1_now;

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
