#ifndef FLIP_INPUT_H
#define FLIP_INPUT_H

#include <stdint.h>

typedef enum {
    FLIP_BTN_UP = 0,
    FLIP_BTN_DOWN,
    FLIP_BTN_LEFT,
    FLIP_BTN_RIGHT,

    FLIP_BTN_A,
    FLIP_BTN_B,
    FLIP_BTN_X,
    FLIP_BTN_Y,

    FLIP_BTN_L1,
    FLIP_BTN_R1,
    FLIP_BTN_L2,
    FLIP_BTN_R2,

    FLIP_BTN_START,
    FLIP_BTN_SELECT,
    FLIP_BTN_MENU,

    FLIP_BTN_L3,
    FLIP_BTN_R3,

    FLIP_BTN_VOL_UP,
    FLIP_BTN_VOL_DOWN,

    FLIP_BTN_COUNT
} flip_button_t;

typedef struct {
    uint8_t down[FLIP_BTN_COUNT];

    int raw_lx;
    int raw_ly;
    int raw_rx;
    int raw_ry;

    int centre_lx;
    int centre_ly;
    int centre_rx;
    int centre_ry;

    int analog_ready;

    /*
     * Hall/lid state.
     *
     * Confirmed change/wake source:
     *   /dev/input/event1
     *   name="hall wake key"
     *   EV_KEY code=116
     *   emits a momentary press/release pulse
     *
     * Authoritative state:
     *   /sys/devices/platform/hall-mh248/hallvalue
     *
     * Confirmed polarity:
     *   hallvalue=1 -> lid open
     *   hallvalue=0 -> lid closed
     */
    int lid_closed;
    int lid_ready;

} flip_input_state_t;

int flip_input_open(void);
void flip_input_close(void);

int flip_input_poll(
    flip_input_state_t *state,
    int timeout_ms);

const char *flip_button_name(
    flip_button_t button);

#endif
