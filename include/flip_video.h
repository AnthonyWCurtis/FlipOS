#ifndef FLIP_VIDEO_H
#define FLIP_VIDEO_H

#include "flip_telemetry.h"

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


typedef struct {
    int selected;
    int volume_percent;
    int brightness_percent;
    int performance_enabled;
    const char *status;
} flip_video_quick_menu_view_t;

/*
 * In-game Quick Menu composition.
 *
 * begin() snapshots the last presented game frame. present() redraws the
 * menu from that immutable snapshot so navigation never accumulates drawing
 * artifacts. end() restores the game frame before emulation resumes.
 */
int flip_video_quick_menu_begin(void);

int flip_video_quick_menu_present(
    const flip_video_quick_menu_view_t *view);

void flip_video_quick_menu_end(void);

typedef struct {
    int selected;
    int confirm_action;
    const char *status;
} flip_video_power_menu_view_t;

int flip_video_power_menu_begin(void);

int flip_video_power_menu_present(
    const flip_video_power_menu_view_t *view);

void flip_video_power_menu_end(void);

int flip_video_submit(
    const void *data,
    unsigned width,
    unsigned height,
    size_t pitch,
    flip_video_format_t format);

/*
 * Temporary diagnostic API retained for compatibility.
 * New emulator-host code should update the full performance
 * overlay snapshot through flip_video_set_perf_overlay().
 */
void flip_video_set_fps_overlay(
    int enabled,
    double fps);

void flip_video_set_perf_overlay(
    int enabled,
    const flip_telemetry_snapshot_t *snapshot);

#endif
