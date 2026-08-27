#define _POSIX_C_SOURCE 200809L

#include "flip_frontend_screensaver.h"

#include "flip_draw.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static int centred_text_x(
    const char *text,
    int scale)
{
    if (!text || scale <= 0)
        return 0;

    int width =
        (int)strlen(text) *
        6 *
        scale;

    return (640 - width) / 2;
}

int flip_frontend_screensaver_draw(
    flip_drm_t *drm,
    double now)
{
    (void)now;

    if (!drm)
        return -1;

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

    /*
     * Shift the clock block each minute so the same pixels are
     * not continuously occupied.
     */
    int slot =
        local_tm.tm_min % 5;

    static const int y_offsets[] = {
        145,
        175,
        205,
        160,
        190,
    };

    int clock_y =
        y_offsets[slot];

    flip_draw_clear(
        drm,
        0x000000);

    flip_draw_text(
        drm,
        centred_text_x(
            clock_text,
            6),
        clock_y,
        clock_text,
        6,
        0xffffff);

    flip_draw_text(
        drm,
        centred_text_x(
            "FLIPOS",
            2),
        clock_y + 70,
        "FLIPOS",
        2,
        0xffffff);

    return 0;
}
