#include "flip_drm.h"
#include "flip_draw.h"
#include "flip_input.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BG      0x00101010
#define FG      0x00ffffff
#define DIM     0x00606060
#define PLAYER  0x00ffffff
#define TARGET  0x00909090

#define PLAYER_SIZE 18
#define TARGET_SIZE 14
#define STEP        8

static volatile sig_atomic_t running = 1;

static void stop_handler(int sig)
{
    (void)sig;
    running = 0;
}

static int clamp_int(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static int overlap(
    int ax, int ay, int aw, int ah,
    int bx, int by, int bw, int bh)
{
    return ax < bx + bw &&
           ax + aw > bx &&
           ay < by + bh &&
           ay + ah > by;
}

static void new_target(int *x, int *y)
{
    *x = 40 + rand() % (640 - 80 - TARGET_SIZE);
    *y = 80 + rand() % (480 - 130 - TARGET_SIZE);
}

static void draw_game(
    flip_drm_t *drm,
    int px,
    int py,
    int tx,
    int ty,
    int score)
{
    flip_draw_clear(drm, BG);

    flip_draw_text(
        drm,
        20, 18,
        "FLIPOS FIRST GAME",
        3,
        FG);

    char score_text[64];

    snprintf(
        score_text,
        sizeof(score_text),
        "SCORE %d",
        score);

    flip_draw_text(
        drm,
        500, 24,
        score_text,
        1,
        FG);

    flip_draw_rect_outline(
        drm,
        20, 62,
        600, 350,
        2,
        DIM);

    /* Target */
    flip_draw_rect(
        drm,
        tx,
        ty,
        TARGET_SIZE,
        TARGET_SIZE,
        TARGET);

    flip_draw_rect_outline(
        drm,
        tx - 3,
        ty - 3,
        TARGET_SIZE + 6,
        TARGET_SIZE + 6,
        1,
        FG);

    /* Player */
    flip_draw_rect(
        drm,
        px,
        py,
        PLAYER_SIZE,
        PLAYER_SIZE,
        PLAYER);

    flip_draw_text(
        drm,
        135, 432,
        "DPAD MOVE   COLLECT TARGETS   MENU EXIT",
        1,
        DIM);

    flip_drm_present(drm);
}

int main(void)
{
    signal(SIGINT, stop_handler);
    signal(SIGTERM, stop_handler);

    srand((unsigned)time(NULL));

    flip_drm_t drm;

    if (flip_drm_open(&drm) < 0) {
        fprintf(stderr, "TESTGAME: DRM init failed\n");
        return 1;
    }

    flip_input_state_t state;
    memset(&state, 0, sizeof(state));

    if (flip_input_open() < 0) {
        fprintf(stderr, "TESTGAME: input init failed\n");
        flip_drm_close(&drm);
        return 2;
    }

    int px = 320 - PLAYER_SIZE / 2;
    int py = 235 - PLAYER_SIZE / 2;

    int tx;
    int ty;

    new_target(&tx, &ty);

    int score = 0;

    draw_game(
        &drm,
        px, py,
        tx, ty,
        score);

    while (running) {

        int changed =
            flip_input_poll(&state, 20);

        if (changed < 0) {
            fprintf(stderr, "TESTGAME: input poll failed\n");
            break;
        }

        /*
         * MENU returns to FlipOS.
         */
        if (state.down[FLIP_BTN_MENU]) {
            fprintf(
                stderr,
                "TESTGAME_EXIT score=%d\n",
                score);
            break;
        }

        int moved = 0;

        if (state.down[FLIP_BTN_LEFT]) {
            px -= STEP;
            moved = 1;
        }

        if (state.down[FLIP_BTN_RIGHT]) {
            px += STEP;
            moved = 1;
        }

        if (state.down[FLIP_BTN_UP]) {
            py -= STEP;
            moved = 1;
        }

        if (state.down[FLIP_BTN_DOWN]) {
            py += STEP;
            moved = 1;
        }

        px = clamp_int(
            px,
            24,
            616 - PLAYER_SIZE);

        py = clamp_int(
            py,
            66,
            408 - PLAYER_SIZE);

        if (overlap(
                px, py,
                PLAYER_SIZE, PLAYER_SIZE,
                tx, ty,
                TARGET_SIZE, TARGET_SIZE)) {

            score++;

            fprintf(
                stderr,
                "TESTGAME_SCORE %d\n",
                score);

            new_target(&tx, &ty);
            moved = 1;
        }

        if (moved) {
            draw_game(
                &drm,
                px, py,
                tx, ty,
                score);
        }
    }

    flip_input_close();
    flip_drm_close(&drm);

    return 0;
}
