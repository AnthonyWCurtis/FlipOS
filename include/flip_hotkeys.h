#ifndef FLIP_HOTKEYS_H
#define FLIP_HOTKEYS_H

#include "flip_input.h"

typedef struct {
    int previous_vol_up;
    int previous_vol_down;
} flip_hotkeys_t;

typedef struct {
    int menu_consumed;
    int volume_changed;
    int brightness_changed;
} flip_hotkeys_result_t;

void flip_hotkeys_init(
    flip_hotkeys_t *hotkeys);

int flip_hotkeys_handle(
    flip_hotkeys_t *hotkeys,
    const flip_input_state_t *input,
    flip_hotkeys_result_t *result);

#endif
