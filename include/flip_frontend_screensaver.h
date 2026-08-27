#ifndef FLIP_FRONTEND_SCREENSAVER_H
#define FLIP_FRONTEND_SCREENSAVER_H

#include "flip_drm.h"

typedef struct {
    int x;
    int y;
    int dx;
    int dy;
} flip_frontend_screensaver_motion_t;

void flip_frontend_screensaver_motion_init(
    flip_frontend_screensaver_motion_t *motion,
    int x,
    int y,
    int dx,
    int dy);

void flip_frontend_screensaver_motion_step(
    flip_frontend_screensaver_motion_t *motion,
    int screen_width,
    int screen_height,
    int block_width,
    int block_height);


int flip_frontend_screensaver_draw(
    flip_drm_t *drm,
    double now);

#endif
