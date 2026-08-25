#ifndef FLIP_OSD_H
#define FLIP_OSD_H

#include "flip_drm.h"

typedef enum {
    FLIP_OSD_NONE = 0,
    FLIP_OSD_VOLUME,
    FLIP_OSD_BRIGHTNESS,
    FLIP_OSD_STATUS
} flip_osd_type_t;

void flip_osd_show_volume(
    int volume);

void flip_osd_show_brightness(
    int brightness);

/*
 * Show a short generic FlipOS status message.
 *
 * Intended for transient system feedback such as:
 *
 *   UPDATING LIBRARY
 *   LIBRARY UPDATED
 *   STATE SAVED
 *
 * The text is copied internally, so the caller does
 * not need to keep the source buffer alive.
 */
void flip_osd_show_status(
    const char *text);

/*
 * Power-off progress is externally driven.
 *
 * progress:
 *   0.0 = hidden
 *   1.0 = complete
 */
void flip_osd_set_poweroff_progress(
    double progress);

int flip_osd_active(void);

int flip_osd_draw(
    flip_drm_t *drm);

#endif
