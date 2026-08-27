#ifndef FLIP_FRONTEND_GLOBAL_CONTROLS_H
#define FLIP_FRONTEND_GLOBAL_CONTROLS_H

#include "flip_hotkeys.h"
#include "flip_input.h"

typedef struct {
    flip_hotkeys_t hotkeys;
    int poweroff_active;
    double poweroff_started;
} flip_frontend_global_controls_t;

typedef struct {
    int redraw;
    int poweroff_active;
    double poweroff_progress;
    int shutdown_requested;
} flip_frontend_global_controls_result_t;

void flip_frontend_global_controls_init(
    flip_frontend_global_controls_t *controls);

int flip_frontend_global_controls_handle(
    flip_frontend_global_controls_t *controls,
    const flip_input_state_t *input,
    flip_frontend_global_controls_result_t *result);

#endif
