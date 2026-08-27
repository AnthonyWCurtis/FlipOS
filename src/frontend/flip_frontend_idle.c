#define _POSIX_C_SOURCE 200809L
#include "flip_frontend_idle.h"

#include <string.h>
#include <time.h>

double flip_frontend_idle_now(void)
{
    struct timespec ts;

    if (clock_gettime(
            CLOCK_MONOTONIC,
            &ts) != 0)
        return 0.0;

    return (double)ts.tv_sec +
           (double)ts.tv_nsec /
               1000000000.0;
}

static flip_frontend_idle_t global_idle;
static int global_idle_initialized = 0;
static double global_idle_last_redraw = 0.0;

static void ensure_global_idle(void)
{
    if (global_idle_initialized)
        return;

    flip_frontend_idle_init(
        &global_idle,
        flip_frontend_idle_now());

    global_idle_initialized = 1;
}

void flip_frontend_idle_global_reset(void)
{
    flip_frontend_idle_init(
        &global_idle,
        flip_frontend_idle_now());

    global_idle_last_redraw = 0.0;
    global_idle_initialized = 1;
}

int flip_frontend_idle_global_process_input(
    flip_input_state_t *input)
{
    ensure_global_idle();

    return flip_frontend_idle_process_input(
        &global_idle,
        input);
}

int flip_frontend_idle_global_active(void)
{
    ensure_global_idle();

    return flip_frontend_idle_active(
        &global_idle);
}

int flip_frontend_idle_global_redraw_due(void)
{
    ensure_global_idle();

    if (!flip_frontend_idle_active(
            &global_idle))
        return 0;

    double now =
        flip_frontend_idle_now();

    if (global_idle_last_redraw == 0.0 ||
        now - global_idle_last_redraw >= 1.0) {

        global_idle_last_redraw = now;
        return 1;
    }

    return 0;
}

flip_frontend_idle_t *
flip_frontend_idle_global_state(void)
{
    ensure_global_idle();

    return &global_idle;
}

static int any_button_down(
    const flip_input_state_t *input)
{
    if (!input)
        return 0;

    for (int i = 0;
         i < FLIP_BTN_COUNT;
         ++i) {

        if (input->down[i])
            return 1;
    }

    return 0;
}

void flip_frontend_idle_init(
    flip_frontend_idle_t *idle,
    double now)
{
    if (!idle)
        return;

    memset(
        idle,
        0,
        sizeof(*idle));

    idle->last_activity = now;
}

int flip_frontend_idle_update(
    flip_frontend_idle_t *idle,
    const flip_input_state_t *input,
    double now)
{
    if (!idle ||
        !input)
        return -1;

    int input_active =
        any_button_down(input);

    /*
     * While consuming the wake gesture, keep consuming until
     * every button has been released.
     */
    if (idle->wake_consuming) {

        if (!input_active) {
            idle->wake_consuming = 0;
            idle->last_activity = now;
        }

        return 0;
    }

    /*
     * Any button while the screensaver is active wakes it and
     * begins wake-input consumption.
     */
    if (idle->active &&
        input_active) {

        idle->active = 0;
        idle->wake_consuming = 1;
        idle->last_activity = now;

        return 0;
    }

    /*
     * Normal activity resets the idle timer.
     */
    if (!idle->active &&
        input_active) {

        idle->last_activity = now;
        return 0;
    }

    if (!idle->active &&
        now - idle->last_activity >=
            FLIP_FRONTEND_IDLE_SECONDS) {

        idle->active = 1;
    }

    return 0;
}

int flip_frontend_idle_process_input(
    flip_frontend_idle_t *idle,
    flip_input_state_t *input)
{
    if (!idle ||
        !input)
        return -1;

    double now =
        flip_frontend_idle_now();

    int was_active =
        flip_frontend_idle_active(
            idle);

    if (flip_frontend_idle_update(
            idle,
            input,
            now) < 0)
        return -1;

    /*
     * The button that wakes the screensaver must not also
     * activate the currently selected frontend action.
     */
    if (was_active &&
        !flip_frontend_idle_active(
            idle)) {

        memset(
            input->down,
            0,
            sizeof(input->down));

        return 1;
    }

    if (flip_frontend_idle_consume_wake(
            idle,
            input)) {

        memset(
            input->down,
            0,
            sizeof(input->down));

        return 1;
    }

    return 0;
}

int flip_frontend_idle_active(
    const flip_frontend_idle_t *idle)
{
    if (!idle)
        return 0;

    return idle->active;
}

int flip_frontend_idle_consume_wake(
    const flip_frontend_idle_t *idle,
    const flip_input_state_t *input)
{
    if (!idle ||
        !input)
        return 0;

    if (!idle->wake_consuming)
        return 0;

    return any_button_down(input);
}
