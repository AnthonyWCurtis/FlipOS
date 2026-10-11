#ifndef FLIP_HOTKEYS_H
#define FLIP_HOTKEYS_H

#include <stdint.h>

#include "flip_controls.h"
#include "flip_input.h"

typedef struct {
    int previous_vol_up;
    int previous_vol_down;
    uint64_t vol_up_repeat_at_ms;
    uint64_t vol_down_repeat_at_ms;
} flip_hotkeys_t;

typedef struct {
    int menu_consumed;
    int volume_changed;
    int brightness_changed;
} flip_hotkeys_result_t;

void flip_hotkeys_init(
    flip_hotkeys_t *hotkeys);

int flip_hotkeys_apply_actions(
    const flip_system_actions_t *actions,
    flip_hotkeys_result_t *result);

int flip_hotkeys_handle(
    flip_hotkeys_t *hotkeys,
    const flip_input_state_t *input,
    flip_hotkeys_result_t *result);

#endif
