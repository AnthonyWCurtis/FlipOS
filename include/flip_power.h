#ifndef FLIP_POWER_H
#define FLIP_POWER_H

#include "flip_input.h"

/*
 * FlipOS global power management.
 *
 * Confirmed Miyoo Flip hardware:
 *
 *   Backlight:
 *     /sys/class/backlight/backlight/brightness
 *
 *   Suspend:
 *     /sys/power/mem_sleep = deep
 *     /sys/power/state     = mem
 *
 *   Lid state:
 *     hallvalue=1 -> OPEN
 *     hallvalue=0 -> CLOSED
 *
 *   Wake:
 *     hall IRQ 72 wakes RK3566 deep suspend
 *     on lid-open.
 */

typedef enum {
    FLIP_POWER_LID_ERROR = -1,
    FLIP_POWER_LID_UNKNOWN = 0,
    FLIP_POWER_LID_OPEN,
    FLIP_POWER_LID_CLOSED
} flip_power_lid_state_t;

/*
 * Refresh the authoritative hall-backed lid state through
 * the Input service and classify it for Power consumers.
 */
flip_power_lid_state_t flip_power_refresh_lid_state(
    flip_input_state_t *state);

/*
 * Enter RK3566 deep suspend while the lid remains closed.
 *
 * Restores the previous backlight level once the lid opens.
 *
 * Returns:
 *   0  success / lid opened
 *  -1  failure
 */
int flip_power_sleep_until_lid_open(
    flip_input_state_t *state);

int flip_power_set_brightness(
    int brightness);

int flip_power_get_brightness(void);

int flip_power_brightness_to_percent(int brightness);
int flip_power_percent_to_brightness(int percent);

#endif
