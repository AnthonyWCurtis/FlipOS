#include "flip_frontend_idle.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void clear_input(
    flip_input_state_t *input)
{
    memset(
        input,
        0,
        sizeof(*input));
}


static void test_process_input_consumes_wake(void)
{
    flip_frontend_idle_t idle;
    flip_input_state_t input;

    memset(&input, 0, sizeof(input));

    flip_frontend_idle_init(
        &idle,
        flip_frontend_idle_now() -
            FLIP_FRONTEND_IDLE_SECONDS -
            1.0);

    assert(
        flip_frontend_idle_update(
            &idle,
            &input,
            flip_frontend_idle_now()) == 0);

    assert(
        flip_frontend_idle_active(
            &idle));

    input.down[FLIP_BTN_A] = 1;

    assert(
        flip_frontend_idle_process_input(
            &idle,
            &input) == 1);

    assert(
        !flip_frontend_idle_active(
            &idle));

    assert(
        input.down[FLIP_BTN_A] == 0);
}


static void test_global_idle_service(void)
{
    flip_input_state_t input;

    memset(
        &input,
        0,
        sizeof(input));

    flip_frontend_idle_global_reset();

    assert(
        flip_frontend_idle_global_active() == 0);

    /*
     * Global frontend input must use the same singleton
     * idle state across every frontend-owned screen.
     */
    assert(
        flip_frontend_idle_global_process_input(
            &input) == 0);

    /*
     * An ordinary button is accepted normally while awake.
     */
    input.down[FLIP_BTN_DOWN] = 1;

    assert(
        flip_frontend_idle_global_process_input(
            &input) == 0);

    assert(
        input.down[FLIP_BTN_DOWN] == 1);
}

int main(void)
{
    test_global_idle_service();
    test_process_input_consumes_wake();
    flip_frontend_idle_t idle;
    flip_input_state_t input;

    flip_frontend_idle_init(
        &idle,
        0.0);

    clear_input(
        &input);

    /*
     * Still awake before the 30-second threshold.
     */
    assert(
        flip_frontend_idle_update(
            &idle,
            &input,
            29.9) == 0);

    assert(
        flip_frontend_idle_active(
            &idle) == 0);

    /*
     * Exactly at 30 seconds, the screensaver activates.
     */
    assert(
        flip_frontend_idle_update(
            &idle,
            &input,
            30.0) == 0);

    assert(
        flip_frontend_idle_active(
            &idle) == 1);

    /*
     * First wake button exits the screensaver but must be consumed.
     */
    input.down[FLIP_BTN_A] = 1;

    assert(
        flip_frontend_idle_update(
            &idle,
            &input,
            30.1) == 0);

    assert(
        flip_frontend_idle_active(
            &idle) == 0);

    assert(
        flip_frontend_idle_consume_wake(
            &idle,
            &input) == 1);

    /*
     * Held wake button must continue to be consumed.
     */
    assert(
        flip_frontend_idle_update(
            &idle,
            &input,
            30.2) == 0);

    assert(
        flip_frontend_idle_consume_wake(
            &idle,
            &input) == 1);

    /*
     * Once all buttons are released, wake consumption clears.
     */
    input.down[FLIP_BTN_A] = 0;

    assert(
        flip_frontend_idle_update(
            &idle,
            &input,
            30.3) == 0);

    assert(
        flip_frontend_idle_consume_wake(
            &idle,
            &input) == 0);

    /*
     * New input after release is normal activity.
     */
    input.down[FLIP_BTN_DOWN] = 1;

    assert(
        flip_frontend_idle_update(
            &idle,
            &input,
            30.4) == 0);

    assert(
        flip_frontend_idle_active(
            &idle) == 0);

    assert(
        flip_frontend_idle_consume_wake(
            &idle,
            &input) == 0);

    puts("frontend idle tests passed");
    return 0;
}
