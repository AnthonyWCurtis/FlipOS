#ifndef FLIP_VIDEO_H
#define FLIP_VIDEO_H

#include <stddef.h>

typedef enum {
    FLIP_VIDEO_FORMAT_RGB565 = 0,
    FLIP_VIDEO_FORMAT_XRGB8888
} flip_video_format_t;

/*
 * Global FlipOS emulator video host.
 *
 * Emulator adapters submit raw core video frames here.
 * The video host owns:
 *
 *   DRM
 *   framebuffer presentation
 *   format conversion
 *   scaling / centering
 *   diagnostic overlays
 *   global OSD composition
 */
int flip_video_open(void);

void flip_video_close(void);

int flip_video_submit(
    const void *data,
    unsigned width,
    unsigned height,
    size_t pitch,
    flip_video_format_t format);

/*
 * Temporary diagnostic API.
 *
 * FPS measurement will move into the emulator host/timing
 * layer later. For Phase 4A1 this preserves the existing
 * FlipGB FPS overlay without changing behaviour.
 */
void flip_video_set_fps_overlay(
    int enabled,
    double fps);

#endif
