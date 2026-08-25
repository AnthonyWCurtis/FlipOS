#include "flip_drm.h"
#include "flip_draw.h"
#include "flip_input.h"

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define BG       0x00101010
#define FG       0x00ffffff
#define DIM      0x00505050
#define TESTED   0x00a0a0a0
#define ACTIVE   0x00ffffff
#define STICK_BG 0x00202020

#define ANALOG_THRESHOLD 35

static volatile sig_atomic_t running = 1;

static unsigned char tested[FLIP_BTN_COUNT];

enum {
    ANALOG_LEFT_UP = 0,
    ANALOG_LEFT_DOWN,
    ANALOG_LEFT_LEFT,
    ANALOG_LEFT_RIGHT,
    ANALOG_RIGHT_UP,
    ANALOG_RIGHT_DOWN,
    ANALOG_RIGHT_LEFT,
    ANALOG_RIGHT_RIGHT,
    ANALOG_TEST_COUNT
};

static unsigned char analog_tested[ANALOG_TEST_COUNT];

typedef struct {
    int initialized;

    int min_lx;
    int max_lx;
    int min_ly;
    int max_ly;

    int min_rx;
    int max_rx;
    int min_ry;
    int max_ry;
} analog_extrema_t;

static analog_extrema_t extrema;

static void stop_handler(int sig)
{
    (void)sig;
    running = 0;
}

static int tested_count(void)
{
    int count = 0;

    for (int i = 0; i < FLIP_BTN_COUNT; ++i) {
        if (tested[i])
            count++;
    }

    return count;
}

static int analog_tested_count(void)
{
    int count = 0;

    for (int i = 0; i < ANALOG_TEST_COUNT; ++i) {
        if (analog_tested[i])
            count++;
    }

    return count;
}

static void update_tested(
    const flip_input_state_t *state)
{
    for (int i = 0; i < FLIP_BTN_COUNT; ++i) {
        if (state->down[i])
            tested[i] = 1;
    }
}

static void update_analog_tests(
    const flip_input_state_t *state)
{
    if (!state->analog_ready)
        return;

    int dlx =
        state->raw_lx - state->centre_lx;

    int dly =
        state->raw_ly - state->centre_ly;

    int drx =
        state->raw_rx - state->centre_rx;

    int dry =
        state->raw_ry - state->centre_ry;

    if (dly <= -ANALOG_THRESHOLD)
        analog_tested[ANALOG_LEFT_UP] = 1;

    if (dly >= ANALOG_THRESHOLD)
        analog_tested[ANALOG_LEFT_DOWN] = 1;

    if (dlx <= -ANALOG_THRESHOLD)
        analog_tested[ANALOG_LEFT_LEFT] = 1;

    if (dlx >= ANALOG_THRESHOLD)
        analog_tested[ANALOG_LEFT_RIGHT] = 1;

    if (dry <= -ANALOG_THRESHOLD)
        analog_tested[ANALOG_RIGHT_UP] = 1;

    if (dry >= ANALOG_THRESHOLD)
        analog_tested[ANALOG_RIGHT_DOWN] = 1;

    if (drx <= -ANALOG_THRESHOLD)
        analog_tested[ANALOG_RIGHT_LEFT] = 1;

    if (drx >= ANALOG_THRESHOLD)
        analog_tested[ANALOG_RIGHT_RIGHT] = 1;

    if (!extrema.initialized) {
        extrema.min_lx = extrema.max_lx = state->raw_lx;
        extrema.min_ly = extrema.max_ly = state->raw_ly;
        extrema.min_rx = extrema.max_rx = state->raw_rx;
        extrema.min_ry = extrema.max_ry = state->raw_ry;
        extrema.initialized = 1;
        return;
    }

    if (state->raw_lx < extrema.min_lx)
        extrema.min_lx = state->raw_lx;
    if (state->raw_lx > extrema.max_lx)
        extrema.max_lx = state->raw_lx;

    if (state->raw_ly < extrema.min_ly)
        extrema.min_ly = state->raw_ly;
    if (state->raw_ly > extrema.max_ly)
        extrema.max_ly = state->raw_ly;

    if (state->raw_rx < extrema.min_rx)
        extrema.min_rx = state->raw_rx;
    if (state->raw_rx > extrema.max_rx)
        extrema.max_rx = state->raw_rx;

    if (state->raw_ry < extrema.min_ry)
        extrema.min_ry = state->raw_ry;
    if (state->raw_ry > extrema.max_ry)
        extrema.max_ry = state->raw_ry;
}

static void button_box(
    flip_drm_t *drm,
    const flip_input_state_t *state,
    flip_button_t button,
    int x,
    int y,
    int w,
    int h)
{
    uint32_t colour;

    if (state->down[button])
        colour = ACTIVE;
    else if (tested[button])
        colour = TESTED;
    else
        colour = DIM;

    flip_draw_rect_outline(
        drm,
        x, y,
        w, h,
        2,
        colour);

    flip_draw_text(
        drm,
        x + 6,
        y + 7,
        flip_button_name(button),
        2,
        colour);

    if (tested[button]) {
        flip_draw_rect(
            drm,
            x + w - 8,
            y + 4,
            4,
            4,
            colour);
    }
}

static int clamp_int(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void analog_marker(
    flip_drm_t *drm,
    int x,
    int y,
    const char *text,
    int tested_state)
{
    uint32_t colour =
        tested_state ? TESTED : DIM;

    flip_draw_text(
        drm,
        x,
        y,
        text,
        1,
        colour);

    if (tested_state) {
        flip_draw_rect(
            drm,
            x - 7,
            y + 2,
            4,
            4,
            colour);
    }
}

static void stick_box(
    flip_drm_t *drm,
    const char *label,
    int x,
    int y,
    int raw_x,
    int raw_y,
    int centre_x,
    int centre_y,
    int ready,
    int base_test)
{
    const int size = 120;

    flip_draw_text(
        drm,
        x,
        y - 18,
        label,
        2,
        FG);

    flip_draw_rect(
        drm,
        x,
        y,
        size,
        size,
        STICK_BG);

    flip_draw_rect_outline(
        drm,
        x,
        y,
        size,
        size,
        2,
        DIM);

    flip_draw_rect(
        drm,
        x + size / 2,
        y + 6,
        1,
        size - 12,
        DIM);

    flip_draw_rect(
        drm,
        x + 6,
        y + size / 2,
        size - 12,
        1,
        DIM);

    if (ready) {
        int dx = raw_x - centre_x;
        int dy = raw_y - centre_y;

        int px =
            size / 2 + dx / 2;

        int py =
            size / 2 + dy / 2;

        px = clamp_int(px, 8, size - 9);
        py = clamp_int(py, 8, size - 9);

        flip_draw_rect(
            drm,
            x + px - 4,
            y + py - 4,
            9,
            9,
            FG);
    }

    char text[64];

    if (!ready) {
        flip_draw_text(
            drm,
            x,
            y + size + 8,
            "CALIBRATING",
            1,
            FG);

        return;
    }

    snprintf(
        text,
        sizeof(text),
        "X:%d C:%d",
        raw_x,
        centre_x);

    flip_draw_text(
        drm,
        x,
        y + size + 8,
        text,
        1,
        FG);

    snprintf(
        text,
        sizeof(text),
        "Y:%d C:%d",
        raw_y,
        centre_y);

    flip_draw_text(
        drm,
        x,
        y + size + 19,
        text,
        1,
        FG);

    analog_marker(
        drm,
        x + 44,
        y - 9,
        "UP",
        analog_tested[base_test + 0]);

    analog_marker(
        drm,
        x + 39,
        y + size - 9,
        "DOWN",
        analog_tested[base_test + 1]);

    analog_marker(
        drm,
        x - 30,
        y + 57,
        "LEFT",
        analog_tested[base_test + 2]);

    analog_marker(
        drm,
        x + size + 5,
        y + 57,
        "RIGHT",
        analog_tested[base_test + 3]);
}

static void draw_screen(
    flip_drm_t *drm,
    const flip_input_state_t *state)
{
    flip_draw_clear(drm, BG);

    flip_draw_text(
        drm,
        145,
        18,
        "FLIPOS HARDWARE TEST",
        3,
        FG);

    char progress[64];

    snprintf(
        progress,
        sizeof(progress),
        "DIGITAL %d/%d   ANALOG %d/%d",
        tested_count(),
        FLIP_BTN_COUNT,
        analog_tested_count(),
        ANALOG_TEST_COUNT);

    flip_draw_text(
        drm,
        205,
        50,
        progress,
        1,
        TESTED);

    button_box(
        drm, state,
        FLIP_BTN_UP,
        60, 90, 70, 34);

    button_box(
        drm, state,
        FLIP_BTN_LEFT,
        18, 128, 80, 34);

    button_box(
        drm, state,
        FLIP_BTN_RIGHT,
        102, 128, 85, 34);

    button_box(
        drm, state,
        FLIP_BTN_DOWN,
        60, 166, 70, 34);

    button_box(
        drm, state,
        FLIP_BTN_Y,
        500, 90, 55, 34);

    button_box(
        drm, state,
        FLIP_BTN_X,
        455, 128, 55, 34);

    button_box(
        drm, state,
        FLIP_BTN_B,
        545, 128, 55, 34);

    button_box(
        drm, state,
        FLIP_BTN_A,
        500, 166, 55, 34);

    button_box(
        drm, state,
        FLIP_BTN_L1,
        205, 86, 55, 32);

    button_box(
        drm, state,
        FLIP_BTN_L2,
        265, 86, 55, 32);

    button_box(
        drm, state,
        FLIP_BTN_R2,
        325, 86, 55, 32);

    button_box(
        drm, state,
        FLIP_BTN_R1,
        385, 86, 55, 32);

    button_box(
        drm, state,
        FLIP_BTN_SELECT,
        205, 132, 90, 34);

    button_box(
        drm, state,
        FLIP_BTN_MENU,
        300, 132, 70, 34);

    button_box(
        drm, state,
        FLIP_BTN_START,
        375, 132, 85, 34);

    button_box(
        drm, state,
        FLIP_BTN_L3,
        218, 176, 55, 32);

    button_box(
        drm, state,
        FLIP_BTN_R3,
        392, 176, 55, 32);

    button_box(
        drm, state,
        FLIP_BTN_VOL_UP,
        205, 216, 90, 32);

    button_box(
        drm, state,
        FLIP_BTN_VOL_DOWN,
        365, 216, 90, 32);

    stick_box(
        drm,
        "LEFT STICK",
        115,
        290,
        state->raw_lx,
        state->raw_ly,
        state->centre_lx,
        state->centre_ly,
        state->analog_ready,
        ANALOG_LEFT_UP);

    stick_box(
        drm,
        "RIGHT STICK",
        405,
        290,
        state->raw_rx,
        state->raw_ry,
        state->centre_rx,
        state->centre_ry,
        state->analog_ready,
        ANALOG_RIGHT_UP);

    char lid_text[64];

    if (!state->lid_ready) {
        snprintf(
            lid_text,
            sizeof(lid_text),
            "LID: WAITING");
    } else {
        snprintf(
            lid_text,
            sizeof(lid_text),
            "LID: %s",
            state->lid_closed ? "CLOSED" : "OPEN");
    }

    flip_draw_text(
        drm,
        20,
        452,
        lid_text,
        1,
        state->lid_ready ? TESTED : DIM);

    flip_draw_text(
        drm,
        238,
        452,
        "MENU + START TO EXIT",
        1,
        DIM);
}

int main(void)
{
    signal(SIGINT, stop_handler);
    signal(SIGTERM, stop_handler);

    flip_drm_t drm;

    if (flip_drm_open(&drm) < 0) {
        fprintf(
            stderr,
            "HWTEST: DRM initialization failed\n");
        return 1;
    }

    flip_input_state_t state;
    memset(&state, 0, sizeof(state));

    memset(tested, 0, sizeof(tested));
    memset(analog_tested, 0, sizeof(analog_tested));
    memset(&extrema, 0, sizeof(extrema));

    if (flip_input_open() < 0) {
        fprintf(
            stderr,
            "HWTEST: input initialization failed\n");

        flip_drm_close(&drm);
        return 2;
    }

    draw_screen(&drm, &state);
    flip_drm_present(&drm);

    while (running) {

        int changed =
            flip_input_poll(&state, 50);

        if (changed < 0) {
            fprintf(
                stderr,
                "HWTEST: input poll failed\n");
            break;
        }

        if (changed > 0) {
            update_tested(&state);
            update_analog_tests(&state);

            draw_screen(
                &drm,
                &state);

            flip_drm_present(&drm);
        }

        if (state.down[FLIP_BTN_MENU] &&
            state.down[FLIP_BTN_START]) {
            break;
        }
    }

    fprintf(
        stderr,
        "HWTEST: digital_tested=%d/%d\n",
        tested_count(),
        FLIP_BTN_COUNT);

    fprintf(
        stderr,
        "HWTEST: analog_tested=%d/%d\n",
        analog_tested_count(),
        ANALOG_TEST_COUNT);

    for (int i = 0;
         i < FLIP_BTN_COUNT;
         ++i) {

        fprintf(
            stderr,
            "HWTEST_BUTTON %s tested=%d\n",
            flip_button_name((flip_button_t)i),
            tested[i] ? 1 : 0);
    }

    fprintf(
        stderr,
        "HWTEST_ANALOG LEFT_UP=%d LEFT_DOWN=%d LEFT_LEFT=%d LEFT_RIGHT=%d "
        "RIGHT_UP=%d RIGHT_DOWN=%d RIGHT_LEFT=%d RIGHT_RIGHT=%d\n",
        analog_tested[ANALOG_LEFT_UP],
        analog_tested[ANALOG_LEFT_DOWN],
        analog_tested[ANALOG_LEFT_LEFT],
        analog_tested[ANALOG_LEFT_RIGHT],
        analog_tested[ANALOG_RIGHT_UP],
        analog_tested[ANALOG_RIGHT_DOWN],
        analog_tested[ANALOG_RIGHT_LEFT],
        analog_tested[ANALOG_RIGHT_RIGHT]);

    if (state.analog_ready) {
        fprintf(
            stderr,
            "HWTEST_ANALOG_CENTER "
            "LX=%d LY=%d RX=%d RY=%d\n",
            state.centre_lx,
            state.centre_ly,
            state.centre_rx,
            state.centre_ry);
    }

    if (extrema.initialized) {
        fprintf(
            stderr,
            "HWTEST_ANALOG_EXTREMA "
            "LX=%d..%d LY=%d..%d "
            "RX=%d..%d RY=%d..%d\n",
            extrema.min_lx,
            extrema.max_lx,
            extrema.min_ly,
            extrema.max_ly,
            extrema.min_rx,
            extrema.max_rx,
            extrema.min_ry,
            extrema.max_ry);
    }

    fprintf(
        stderr,
        "HWTEST_LID ready=%d closed=%d\n",
        state.lid_ready,
        state.lid_closed);

    flip_input_close();
    flip_drm_close(&drm);

    return 0;
}
