#ifndef FLIP_CONTROLS_H
#define FLIP_CONTROLS_H

#include "flip_input.h"

#include <stdint.h>

typedef enum {
    FLIP_GAME_UP = 0,
    FLIP_GAME_DOWN,
    FLIP_GAME_LEFT,
    FLIP_GAME_RIGHT,

    FLIP_GAME_A,
    FLIP_GAME_B,
    FLIP_GAME_X,
    FLIP_GAME_Y,

    FLIP_GAME_L1,
    FLIP_GAME_R1,
    FLIP_GAME_L2,
    FLIP_GAME_R2,

    FLIP_GAME_START,
    FLIP_GAME_SELECT,
    FLIP_GAME_L3,
    FLIP_GAME_R3,

    FLIP_GAME_BUTTON_COUNT
} flip_game_button_t;

typedef struct {
    uint8_t down[FLIP_GAME_BUTTON_COUNT];

    int lx;
    int ly;
    int rx;
    int ry;

    int analog_ready;
} flip_game_input_t;

typedef struct {
    int gameswitcher_requested;
    int game_options_requested;
    int fps_toggle_requested;
    int exit_requested;

    int volume_step;
    int brightness_step;

    int lid_ready;
    int lid_closed;
    int lid_changed;
    int suspend_requested;
} flip_system_actions_t;

typedef struct {
    flip_game_input_t game;
    flip_system_actions_t system;
} flip_controls_state_t;

typedef struct {
    int previous_menu;
    int previous_r1;
    int previous_start;
    int previous_select;
    int previous_vol_up;
    int previous_vol_down;

    int previous_lid_ready;
    int previous_lid_closed;

    int menu_pending;
    uint64_t menu_pressed_ms;
    int menu_hold_emitted;
} flip_controls_t;

void flip_controls_init(
    flip_controls_t *controls);

int flip_controls_update_at(
    flip_controls_t *controls,
    const flip_input_state_t *input,
    flip_controls_state_t *state,
    uint64_t now_ms);

int flip_controls_update(
    flip_controls_t *controls,
    const flip_input_state_t *input,
    flip_controls_state_t *state);

#endif
