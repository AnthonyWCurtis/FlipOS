#define _POSIX_C_SOURCE 200809L

#include "flip_frontend_screensaver.h"

#include "flip_draw.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480

#define CLOCK_SCALE 6
#define LOGO_SCALE 2

#define BLOCK_WIDTH 180
#define BLOCK_HEIGHT 110

static flip_frontend_screensaver_motion_t motion;
static int motion_initialized = 0;

void flip_frontend_screensaver_motion_init(
    flip_frontend_screensaver_motion_t *motion,
    int x,
    int y,
    int dx,
    int dy)
{
    if (!motion)
        return;

    motion->x = x;
    motion->y = y;
    motion->dx = dx;
    motion->dy = dy;
}

void flip_frontend_screensaver_motion_step(
    flip_frontend_screensaver_motion_t *motion,
    int screen_width,
    int screen_height,
    int block_width,
    int block_height)
{
    if (!motion)
        return;

    int max_x =
        screen_width -
        block_width;

    int max_y =
        screen_height -
        block_height;

    if (max_x < 0)
        max_x = 0;

    if (max_y < 0)
        max_y = 0;

    if (motion->x <= 0 &&
        motion->dx < 0) {

        motion->dx = -motion->dx;
    }

    if (motion->x >= max_x &&
        motion->dx > 0) {

        motion->dx = -motion->dx;
    }

    if (motion->y <= 0 &&
        motion->dy < 0) {

        motion->dy = -motion->dy;
    }

    if (motion->y >= max_y &&
        motion->dy > 0) {

        motion->dy = -motion->dy;
    }

    motion->x += motion->dx;
    motion->y += motion->dy;
}

static int centred_in_block_x(
    const char *text,
    int scale)
{
    if (!text || scale <= 0)
        return 0;

    int width =
        (int)strlen(text) *
        6 *
        scale;

    return
        motion.x +
        (BLOCK_WIDTH - width) / 2;
}

int flip_frontend_screensaver_draw(
    flip_drm_t *drm,
    double now)
{
    (void)now;

    if (!drm)
        return -1;

    if (!motion_initialized) {

        flip_frontend_screensaver_motion_init(
            &motion,
            80,
            80,
            6,
            4);

        motion_initialized = 1;
    }

    time_t wall_time =
        time(NULL);

    struct tm local_tm;

    if (!localtime_r(
            &wall_time,
            &local_tm))
        return -1;

    char clock_text[16];

    if (strftime(
            clock_text,
            sizeof(clock_text),
            "%H:%M",
            &local_tm) == 0)
        return -1;

    flip_draw_clear(
        drm,
        0x000000);

    flip_draw_text(
        drm,
        centred_in_block_x(
            clock_text,
            CLOCK_SCALE),
        motion.y,
        clock_text,
        CLOCK_SCALE,
        0xffffff);

    flip_draw_text(
        drm,
        centred_in_block_x(
            "FLIPOS",
            LOGO_SCALE),
        motion.y + 72,
        "FLIPOS",
        LOGO_SCALE,
        0xffffff);

    /*
     * Presentation is already throttled to ~1 Hz, so one motion
     * step per draw gives a slow, low-cost DVD-style drift.
     */
    flip_frontend_screensaver_motion_step(
        &motion,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        BLOCK_WIDTH,
        BLOCK_HEIGHT);

    return 0;
}
