#include "flip_draw.h"

void flip_draw_clear(flip_drm_t *drm, uint32_t colour)
{
    (void)drm;
    (void)colour;
}

void flip_draw_rect(
    flip_drm_t *drm,
    int x,
    int y,
    int w,
    int h,
    uint32_t colour)
{
    (void)drm;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)colour;
}

void flip_draw_rect_outline(
    flip_drm_t *drm,
    int x,
    int y,
    int w,
    int h,
    int thickness,
    uint32_t colour)
{
    (void)drm;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)thickness;
    (void)colour;
}

void flip_draw_text(
    flip_drm_t *drm,
    int x,
    int y,
    const char *text,
    int scale,
    uint32_t colour)
{
    (void)drm;
    (void)x;
    (void)y;
    (void)text;
    (void)scale;
    (void)colour;
}
