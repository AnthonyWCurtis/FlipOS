#include "flip_drm.h"
#include "flip_draw.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BG    0x00101010
#define FG    0x00ffffff
#define DIM   0x00505050
#define PANEL 0x00202020

#define CALIBRATION_FILE \
    "/mnt/SDCARD/flipos-dev/state/optimizer/cpu-calibration"
#define MIN_TUNABLE_KHZ 1104000U

static volatile sig_atomic_t running = 1;

static void stop_handler(int sig)
{
    (void)sig;
    running = 0;
}

static int centred_text_x(const char *text, int scale)
{
    int width =
        (int)strlen(text) * 6 * scale;
    return (640 - width) / 2;
}

static void read_progress(
    unsigned *completed,
    unsigned *total)
{
    *completed = 0;
    *total = 0;

    FILE *f = fopen(CALIBRATION_FILE, "r");
    if (!f)
        return;

    unsigned khz[16] = {0};
    int complete[16] = {0};
    char line[256];

    while (fgets(line, sizeof(line), f)) {
        unsigned index = 0;
        unsigned value = 0;

        if (sscanf(
                line,
                "opp%u_khz=%u",
                &index,
                &value) == 2 &&
            index < 16) {
            khz[index] = value;
            continue;
        }

        if (sscanf(
                line,
                "opp%u_complete=%u",
                &index,
                &value) == 2 &&
            index < 16) {
            complete[index] =
                value ? 1 : 0;
        }
    }

    fclose(f);

    for (unsigned i = 0; i < 16; ++i) {
        if (khz[i] < MIN_TUNABLE_KHZ)
            continue;
        (*total)++;
        if (complete[i])
            (*completed)++;
    }
}

int main(void)
{
    signal(SIGINT, stop_handler);
    signal(SIGTERM, stop_handler);

    unsigned completed = 0;
    unsigned total = 0;
    read_progress(&completed, &total);

    if (total == 0)
        total = 5;

    flip_drm_t drm;
    if (flip_drm_open(&drm) < 0)
        return 1;

    flip_draw_clear(&drm, BG);

    flip_draw_text(
        &drm,
        centred_text_x(
            "OPTIMIZING CPU",
            4),
        55,
        "OPTIMIZING CPU",
        4,
        FG);

    flip_draw_text(
        &drm,
        centred_text_x(
            "DO NOT POWER OFF",
            2),
        118,
        "DO NOT POWER OFF",
        2,
        FG);

    char progress_text[96];
    snprintf(
        progress_text,
        sizeof(progress_text),
        "%u / %u PERFORMANCE LEVELS COMPLETE",
        completed,
        total);

    flip_draw_text(
        &drm,
        centred_text_x(
            progress_text,
            1),
        175,
        progress_text,
        1,
        DIM);

    const int bar_x = 90;
    const int bar_y = 212;
    const int bar_w = 460;
    const int bar_h = 34;

    flip_draw_rect(
        &drm,
        bar_x,
        bar_y,
        bar_w,
        bar_h,
        PANEL);

    flip_draw_rect_outline(
        &drm,
        bar_x,
        bar_y,
        bar_w,
        bar_h,
        2,
        FG);

    int fill =
        total > 0
            ? (int)((bar_w - 8) *
                completed / total)
            : 0;

    if (fill > 0) {
        flip_draw_rect(
            &drm,
            bar_x + 4,
            bar_y + 4,
            fill,
            bar_h - 8,
            FG);
    }

    flip_draw_text(
        &drm,
        centred_text_x(
            "FINDING SAFE SETTINGS FOR THIS DEVICE",
            1),
        282,
        "FINDING SAFE SETTINGS FOR THIS DEVICE",
        1,
        DIM);

    flip_draw_text(
        &drm,
        centred_text_x(
            "THE FLIP MAY RESTART SEVERAL TIMES",
            1),
        314,
        "THE FLIP MAY RESTART SEVERAL TIMES",
        1,
        DIM);

    flip_draw_text(
        &drm,
        centred_text_x(
            "KEEP CHARGER CONNECTED",
            2),
        354,
        "KEEP CHARGER CONNECTED",
        2,
        FG);

    flip_draw_text(
        &drm,
        centred_text_x(
            "UNPLUG CHARGER TO CANCEL SAFELY",
            1),
        397,
        "UNPLUG CHARGER TO CANCEL SAFELY",
        1,
        FG);

    flip_draw_text(
        &drm,
        centred_text_x(
            "PROGRESS IS SAVED AUTOMATICALLY",
            1),
        425,
        "PROGRESS IS SAVED AUTOMATICALLY",
        1,
        DIM);

    flip_drm_present(&drm);

    while (running)
        pause();

    flip_drm_close(&drm);
    return 0;
}
