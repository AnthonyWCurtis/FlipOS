#ifndef FLIP_FRONTEND_IDLE_H
#define FLIP_FRONTEND_IDLE_H

#include "flip_input.h"

#define FLIP_FRONTEND_IDLE_SECONDS 30.0

double flip_frontend_idle_now(void);

typedef struct {
    double last_activity;
    int active;
    int wake_consuming;
} flip_frontend_idle_t;

void flip_frontend_idle_init(
    flip_frontend_idle_t *idle,
    double now);

int flip_frontend_idle_update(
    flip_frontend_idle_t *idle,
    const flip_input_state_t *input,
    double now);

int flip_frontend_idle_process_input(
    flip_frontend_idle_t *idle,
    flip_input_state_t *input);


int flip_frontend_idle_active(
    const flip_frontend_idle_t *idle);

int flip_frontend_idle_consume_wake(
    const flip_frontend_idle_t *idle,
    const flip_input_state_t *input);

void flip_frontend_idle_global_reset(void);

int flip_frontend_idle_global_process_input(
    flip_input_state_t *input);

int flip_frontend_idle_global_active(void);

int flip_frontend_idle_global_redraw_due(void);

flip_frontend_idle_t *
flip_frontend_idle_global_state(void);

#endif
