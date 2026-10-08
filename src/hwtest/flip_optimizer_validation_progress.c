#include "flip_draw.h"

#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BG    0x00101010
#define FG    0x00ffffff
#define DIM   0x00505050
#define PANEL 0x00202020

static volatile sig_atomic_t running = 1;

static void stop_handler(int sig)
{
    (void)sig;
    running = 0;
}

static int centred_text_x(const char *text, int scale)
{
    int width = (int)strlen(text) * 6 * scale;
    return (640 - width) / 2;
}

int main(void)
{
    signal(SIGINT, stop_handler);
    signal(SIGTERM, stop_handler);

    flip_drm_t drm;
    if (flip_drm_open(&drm) < 0)
        return 1;

    flip_draw_clear(&drm, BG);

    flip_draw_text(
        &drm,
        centred_text_x("CPU PROFILE STRESS TEST", 3),
        50,
        "CPU PROFILE STRESS TEST",
        3,
        FG);

    flip_draw_text(
        &drm,
        centred_text_x("VALIDATING OPTIMIZED VOLTAGES", 2),
        112,
        "VALIDATING OPTIMIZED VOLTAGES",
        2,
        FG);

    flip_draw_rect(
        &drm,
        95,
        174,
        450,
        72,
        PANEL);

    flip_draw_rect_outline(
        &drm,
        95,
        174,
        450,
        72,
        2,
        FG);

    flip_draw_text(
        &drm,
        centred_text_x("STRESSING ALL PERFORMANCE LEVELS", 1),
        192,
        "STRESSING ALL PERFORMANCE LEVELS",
        1,
        FG);

    flip_draw_text(
        &drm,
        centred_text_x("APPROXIMATELY 5 MINUTES", 1),
        219,
        "APPROXIMATELY 5 MINUTES",
        1,
        DIM);

    flip_draw_text(
        &drm,
        centred_text_x("WATCHDOG RECOVERY IS ACTIVE", 1),
        287,
        "WATCHDOG RECOVERY IS ACTIVE",
        1,
        DIM);

    flip_draw_text(
        &drm,
        centred_text_x("KEEP CHARGER CONNECTED", 2),
        340,
        "KEEP CHARGER CONNECTED",
        2,
        FG);

    flip_draw_text(
        &drm,
        centred_text_x("UNPLUG CHARGER TO CANCEL SAFELY", 1),
        389,
        "UNPLUG CHARGER TO CANCEL SAFELY",
        1,
        FG);

    flip_draw_text(
        &drm,
        centred_text_x("FLIPOS WILL RESTART WHEN FINISHED", 1),
        430,
        "FLIPOS WILL RESTART WHEN FINISHED",
        1,
        DIM);

    flip_drm_present(&drm);

    while (running)
        pause();

    flip_drm_close(&drm);
    return 0;
}
