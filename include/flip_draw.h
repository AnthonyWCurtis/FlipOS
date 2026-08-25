#ifndef FLIP_DRAW_H
#define FLIP_DRAW_H

#include "flip_drm.h"
#include <stdint.h>

void flip_draw_clear(flip_drm_t *drm, uint32_t colour);

void flip_draw_rect(
    flip_drm_t *drm,
    int x,
    int y,
    int w,
    int h,
    uint32_t colour);

void flip_draw_rect_outline(
    flip_drm_t *drm,
    int x,
    int y,
    int w,
    int h,
    int thickness,
    uint32_t colour);

void flip_draw_text(
    flip_drm_t *drm,
    int x,
    int y,
    const char *text,
    int scale,
    uint32_t colour);

#endif
