#ifndef FLIP_HOTKEYS_H
#define FLIP_HOTKEYS_H

#include "flip_controls.h"

typedef struct {
    int menu_consumed;
    int volume_changed;
    int brightness_changed;
} flip_hotkeys_result_t;

int flip_hotkeys_apply_actions(
    const flip_system_actions_t *actions,
    flip_hotkeys_result_t *result);

#endif
