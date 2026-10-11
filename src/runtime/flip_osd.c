#include "flip_osd.h"

#include "flip_draw.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#define OSD_DURATION_MS 1000

#define OSD_BG  0x00000000
#define OSD_FG  0x00ffffff
#define OSD_DIM 0x00505050

static flip_osd_type_t current_type =
    FLIP_OSD_NONE;

static int current_percent = 0;

static char current_status[FLIP_OSD_STATUS_MAX];

static struct timespec shown_at;

static int poweroff_active = 0;
static int poweroff_percent = 0;

static long long elapsed_ms(
    const struct timespec *start,
    const struct timespec *end)
{
    long long sec =
        (long long)end->tv_sec -
        (long long)start->tv_sec;

    long long nsec =
        (long long)end->tv_nsec -
        (long long)start->tv_nsec;

    return
        sec * 1000LL +
        nsec / 1000000LL;
}

static int clamp_percent(
    int percent)
{
    if (percent < 0)
        return 0;

    if (percent > 100)
        return 100;

    return percent;
}

void flip_osd_reset(void)
{
    current_type = FLIP_OSD_NONE;
    current_percent = 0;
    current_status[0] = '\0';
    memset(&shown_at, 0, sizeof(shown_at));
    poweroff_active = 0;
    poweroff_percent = 0;
}

void flip_osd_init(void)
{
    flip_osd_reset();
}

static void show(
    flip_osd_type_t type,
    int percent)
{
    current_type =
        type;

    current_percent =
        clamp_percent(
            percent);

    clock_gettime(
        CLOCK_MONOTONIC,
        &shown_at);
}

void flip_osd_show_volume(
    int volume)
{
    show(
        FLIP_OSD_VOLUME,
        volume);
}

void flip_osd_show_brightness(
    int brightness)
{
    /* Keep display conversion in this dependency-light OSD module. */
    int percent;

    if (brightness <= 10)
        percent = 0;
    else if (brightness >= 255)
        percent = 100;
    else
        percent = ((brightness - 10) * 100 + 122) / 245;

    show(
        FLIP_OSD_BRIGHTNESS,
        percent);
}

void flip_osd_show_status(
    const char *text)
{
    if (!text ||
        !text[0]) {

        current_status[0] = '\0';
        current_type =
            FLIP_OSD_NONE;

        return;
    }

    snprintf(
        current_status,
        sizeof(current_status),
        "%s",
        text);

    current_type =
        FLIP_OSD_STATUS;

    clock_gettime(
        CLOCK_MONOTONIC,
        &shown_at);
}

void flip_osd_set_poweroff_progress(
    double progress)
{
    if (progress <= 0.0) {

        poweroff_active = 0;
        poweroff_percent = 0;

        return;
    }

    if (progress > 1.0)
        progress = 1.0;

    poweroff_active = 1;

    poweroff_percent =
        (int)(
            progress * 100.0 +
            0.5);

    poweroff_percent =
        clamp_percent(
            poweroff_percent);
}

static int transient_active(void)
{
    if (current_type == FLIP_OSD_NONE)
        return 0;

    struct timespec now;

    clock_gettime(
        CLOCK_MONOTONIC,
        &now);

    if (elapsed_ms(
            &shown_at,
            &now) >=
        OSD_DURATION_MS) {

        current_type = FLIP_OSD_NONE;
        current_percent = 0;
        current_status[0] = '\0';

        return 0;
    }

    return 1;
}

int flip_osd_snapshot(
    flip_osd_snapshot_t *snapshot)
{
    if (!snapshot)
        return -1;

    int active =
        poweroff_active ||
        transient_active();

    memset(
        snapshot,
        0,
        sizeof(*snapshot));

    snapshot->type =
        current_type;

    snapshot->percent =
        current_percent;

    snprintf(
        snapshot->status,
        sizeof(snapshot->status),
        "%s",
        current_status);

    snapshot->poweroff_active =
        poweroff_active;

    snapshot->poweroff_percent =
        poweroff_percent;

    return active ? 1 : 0;
}

int flip_osd_active(void)
{
    flip_osd_snapshot_t snapshot;

    int rc =
        flip_osd_snapshot(
            &snapshot);

    return rc > 0
        ? 1
        : 0;
}

int flip_osd_draw(
    flip_drm_t *drm)
{
    if (!drm)
        return 0;

    flip_osd_snapshot_t snapshot;

    if (flip_osd_snapshot(
            &snapshot) <= 0)
        return 0;

    const char *label;
    int percent = 0;
    int status_only = 0;

    if (snapshot.poweroff_active) {

        label =
            "POWER OFF";

        percent =
            snapshot.poweroff_percent;

    } else if (
        snapshot.type ==
            FLIP_OSD_STATUS) {

        label =
            snapshot.status;

        status_only = 1;

    } else {

        label =
            snapshot.type ==
                FLIP_OSD_VOLUME
                ? "VOLUME"
                : "BRIGHTNESS";

        percent =
            snapshot.percent;
    }

    /*
     * Slim FlipOS transient status bar.
     *
     * Intentionally much less intrusive than
     * the original boxed OSD.
     */
    const int panel_x = 110;
    const int panel_y = 406;
    const int panel_w = 420;
    const int panel_h = 36;

    const int bar_x = 250;
    const int bar_y = 420;
    const int bar_w = 220;
    const int bar_h = 7;

    flip_draw_rect(
        drm,
        panel_x,
        panel_y,
        panel_w,
        panel_h,
        OSD_BG);

    /*
     * Thin outer border keeps the global OSD readable
     * over both bright and dark gameplay without turning
     * it back into a chunky popup.
     */
    flip_draw_rect_outline(
        drm,
        panel_x,
        panel_y,
        panel_w,
        panel_h,
        1,
        0x00000000);

    flip_draw_text(
        drm,
        126,
        418,
        label,
        1,
        OSD_FG);

    if (!status_only) {

        flip_draw_rect(
            drm,
            bar_x,
            bar_y,
            bar_w,
            bar_h,
            OSD_DIM);

        int filled =
            (bar_w * percent) /
            100;

        if (filled > 0) {

            flip_draw_rect(
                drm,
                bar_x,
                bar_y,
                filled,
                bar_h,
                OSD_FG);
        }

        flip_draw_rect_outline(
            drm,
            bar_x,
            bar_y,
            bar_w,
            bar_h,
            1,
            OSD_FG);

        /*
         * Percentage is useful for volume/brightness,
         * but power-off progress is clearer without
         * numeric clutter.
         */
        if (!snapshot.poweroff_active) {

            char percent_text[16];

            snprintf(
                percent_text,
                sizeof(percent_text),
                "%d%%",
                percent);

            flip_draw_text(
                drm,
                486,
                418,
                percent_text,
                1,
                OSD_FG);
        }
    }

    return 1;
}
