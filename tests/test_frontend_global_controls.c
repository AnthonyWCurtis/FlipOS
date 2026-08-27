#include "flip_frontend_global_controls.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/*
 * This test exercises the controller state machine without hardware.
 * flip_hotkeys_handle() is stubbed because volume/brightness behavior
 * already has its own tests; here we care about delegation and power
 * hold state.
 */

int flip_hotkeys_handle(
    flip_hotkeys_t *hotkeys,
    const flip_input_state_t *input,
    flip_hotkeys_result_t *result)
{
    (void)hotkeys;
    (void)input;

    assert(result);

    memset(
        result,
        0,
        sizeof(*result));

    return 0;
}

void flip_hotkeys_init(
    flip_hotkeys_t *hotkeys)
{
    assert(hotkeys);

    memset(
        hotkeys,
        0,
        sizeof(*hotkeys));
}

void flip_osd_set_poweroff_progress(
    double progress)
{
    (void)progress;
}

int main(void)
{
    flip_frontend_global_controls_t controls;

    flip_frontend_global_controls_init(
        &controls);

    assert(controls.poweroff_active == 0);

    flip_input_state_t input;
    memset(
        &input,
        0,
        sizeof(input));

    flip_frontend_global_controls_result_t result;

    /*
     * Idle poll.
     */
    assert(
        flip_frontend_global_controls_handle(
            &controls,
            &input,
            &result) == 0);

    assert(result.poweroff_active == 0);
    assert(result.shutdown_requested == 0);

    /*
     * Begin MENU+SELECT hold.
     */
    input.down[FLIP_BTN_MENU] = 1;
    input.down[FLIP_BTN_SELECT] = 1;

    assert(
        flip_frontend_global_controls_handle(
            &controls,
            &input,
            &result) == 0);

    assert(controls.poweroff_active == 1);
    assert(result.poweroff_active == 1);
    assert(result.redraw == 1);
    assert(result.shutdown_requested == 0);

    /*
     * Releasing either button cancels the hold.
     */
    input.down[FLIP_BTN_SELECT] = 0;

    assert(
        flip_frontend_global_controls_handle(
            &controls,
            &input,
            &result) == 0);

    assert(controls.poweroff_active == 0);
    assert(result.poweroff_active == 0);
    assert(result.redraw == 1);
    assert(result.shutdown_requested == 0);

    puts("frontend global controls tests passed");
    return 0;
}
