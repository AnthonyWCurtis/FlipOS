#define _POSIX_C_SOURCE 200809L

#include "flip_draw.h"

#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define BG    0x00101010
#define FG    0x00ffffff
#define DIM   0x00505050
#define PANEL 0x00202020

#define VALIDATION_REQUEST     "/mnt/SDCARD/flipos-dev/state/optimizer/validation-request"
#define VALIDATION_ACTIVE     "/mnt/SDCARD/flipos-dev/state/optimizer/validation-active"
#define VALIDATION_MANUAL_STOP "/mnt/SDCARD/flipos-dev/state/optimizer/validation-manual-stop"

static volatile sig_atomic_t running = 1;

typedef struct {
    unsigned long long total;
    unsigned long long idle;
} cpu_sample_t;

static void stop_handler(int sig)
{
    (void)sig;
    running = 0;
}

static void record_manual_stop(void)
{
    int fd = open(
        VALIDATION_MANUAL_STOP,
        O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,
        0644);
    if (fd < 0)
        return;

    static const char text[] = "power-key\n";
    ssize_t written = write(fd, text, sizeof(text) - 1U);
    if (written == (ssize_t)(sizeof(text) - 1U))
        (void)fsync(fd);
    (void)close(fd);
}

static void poll_power_key(int input_fd)
{
    if (input_fd < 0)
        return;

    for (;;) {
        struct input_event ev;
        ssize_t n = read(input_fd, &ev, sizeof(ev));
        if (n == (ssize_t)sizeof(ev)) {
            if (ev.type == EV_KEY && ev.code == KEY_POWER && ev.value == 1)
                record_manual_stop();
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
            return;
        return;
    }
}

static int centred_text_x(const char *text, int scale)
{
    int width = (int)strlen(text) * 6 * scale;
    return (640 - width) / 2;
}

static unsigned read_duration_minutes(void)
{
    FILE *f = fopen(VALIDATION_REQUEST, "r");
    if (!f)
        return 5U;

    char line[128];
    unsigned minutes = 5U;
    while (fgets(line, sizeof(line), f)) {
        unsigned parsed = 0;
        if (sscanf(line, "duration_minutes=%u", &parsed) == 1) {
            if (parsed == 5U || parsed == 10U || parsed == 20U ||
                parsed == 30U || parsed == 60U)
                minutes = parsed;
            break;
        }
    }

    fclose(f);
    return minutes;
}

static void read_validation_status(
    char *status,
    size_t status_size,
    unsigned *tested_opps)
{
    if (!status || status_size == 0)
        return;

    snprintf(status, status_size, "PREPARING");
    if (tested_opps)
        *tested_opps = 0;

    FILE *f = fopen(VALIDATION_ACTIVE, "r");
    if (!f)
        return;

    char line[160];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "status=", 7) == 0) {
            char *value = line + 7;
            char *end = strpbrk(value, "\r\n");
            if (end)
                *end = '\0';
            snprintf(
                status,
                status_size,
                "%.*s",
                (int)status_size - 1,
                value);
        } else if (tested_opps) {
            unsigned parsed = 0;
            if (sscanf(line, "tested_opps=%u", &parsed) == 1)
                *tested_opps = parsed;
        }
    }

    fclose(f);
}

static int read_cpu_samples(cpu_sample_t samples[4])
{
    FILE *f = fopen("/proc/stat", "r");
    if (!f)
        return -1;

    char line[256];
    int found = 0;
    while (found < 4 && fgets(line, sizeof(line), f)) {
        unsigned cpu = 0;
        unsigned long long user = 0;
        unsigned long long nice = 0;
        unsigned long long system = 0;
        unsigned long long idle = 0;
        unsigned long long iowait = 0;
        unsigned long long irq = 0;
        unsigned long long softirq = 0;
        unsigned long long steal = 0;

        if (sscanf(
                line,
                "cpu%u %llu %llu %llu %llu %llu %llu %llu %llu",
                &cpu,
                &user,
                &nice,
                &system,
                &idle,
                &iowait,
                &irq,
                &softirq,
                &steal) == 9 &&
            cpu < 4U) {
            samples[cpu].idle = idle + iowait;
            samples[cpu].total =
                user + nice + system + idle + iowait + irq + softirq + steal;
            found++;
        }
    }

    fclose(f);
    return found == 4 ? 0 : -1;
}

static unsigned cpu_usage_percent(
    const cpu_sample_t *before,
    const cpu_sample_t *after)
{
    if (!before || !after || after->total <= before->total)
        return 0;

    unsigned long long total = after->total - before->total;
    unsigned long long idle =
        after->idle >= before->idle
            ? after->idle - before->idle
            : 0;

    if (idle >= total)
        return 0;

    return (unsigned)(((total - idle) * 100ULL) / total);
}

static unsigned read_soc_temp_c(void)
{
    FILE *f = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (!f)
        return 0;

    unsigned temp_mc = 0;
    if (fscanf(f, "%u", &temp_mc) != 1)
        temp_mc = 0;

    fclose(f);
    return temp_mc / 1000U;
}

static const char *friendly_phase(const char *status)
{
    if (!status || !status[0])
        return "PREPARING";

    if (strncmp(status, "entering-", 9) == 0)
        return "STRESSING CPU";
    if (strcmp(status, "running") == 0)
        return "PREPARING";
    if (strcmp(status, "arming-quiesce") == 0)
        return "PREPARING SYSTEM";
    if (strcmp(status, "arming-panic-policy") == 0 ||
        strcmp(status, "loading-tsadc-recovery") == 0 ||
        strcmp(status, "arming-tsadc-recovery") == 0 ||
        strcmp(status, "arming-hw-watchdog") == 0 ||
        strcmp(status, "arming-deadman") == 0)
        return "PREPARING SAFETY CHECKS";
    if (strcmp(status, "applying-profile") == 0)
        return "APPLYING TUNED SETTINGS";
    if (strcmp(status, "profile-applied") == 0)
        return "STRESSING CPU";
    return "STRESSING CPU";
}

static void draw_screen(
    flip_drm_t *drm,
    unsigned duration_minutes,
    unsigned elapsed_seconds,
    const unsigned usage[4],
    const char *phase,
    unsigned tested_opps,
    unsigned temp_c)
{
    flip_draw_clear(drm, BG);

    flip_draw_text(
        drm,
        centred_text_x("CPU STABILITY TEST", 3),
        32,
        "CPU STABILITY TEST",
        3,
        FG);

    char subtitle[64];
    snprintf(
        subtitle,
        sizeof(subtitle),
        "%u MINUTE STABILITY TEST",
        duration_minutes);
    flip_draw_text(
        drm,
        centred_text_x(subtitle, 1),
        78,
        subtitle,
        1,
        DIM);

    flip_draw_text(
        drm,
        centred_text_x(phase, 1),
        103,
        phase,
        1,
        FG);

    const int bar_w = 62;
    const int bar_h = 140;
    const int gap = 28;
    const int total_w = 4 * bar_w + 3 * gap;
    const int start_x = (640 - total_w) / 2;
    const int base_y = 285;

    for (int i = 0; i < 4; ++i) {
        int x = start_x + i * (bar_w + gap);
        flip_draw_rect(drm, x, base_y - bar_h, bar_w, bar_h, PANEL);
        flip_draw_rect_outline(drm, x, base_y - bar_h, bar_w, bar_h, 2, FG);

        unsigned clamped = usage[i] > 100U ? 100U : usage[i];
        int fill_h = (int)((clamped * (unsigned)(bar_h - 6)) / 100U);
        if (fill_h > 0)
            flip_draw_rect(
                drm,
                x + 3,
                base_y - 3 - fill_h,
                bar_w - 6,
                fill_h,
                FG);

        char core[16];
        snprintf(core, sizeof(core), "C%d", i);
        flip_draw_text(
            drm,
            x + (bar_w - (int)strlen(core) * 12) / 2,
            300,
            core,
            2,
            FG);

        char percent[16];
        snprintf(percent, sizeof(percent), "%u%%", clamped);
        flip_draw_text(
            drm,
            x + (bar_w - (int)strlen(percent) * 6) / 2,
            328,
            percent,
            1,
            DIM);
    }

    unsigned total_seconds = duration_minutes * 60U;
    unsigned remaining =
        elapsed_seconds >= total_seconds
            ? 0U
            : total_seconds - elapsed_seconds;
    unsigned progress =
        total_seconds == 0
            ? 0U
            : (elapsed_seconds * 100U) / total_seconds;
    if (tested_opps < 5U && progress >= 100U)
        progress = 99U;
    else if (progress > 100U)
        progress = 100U;

    char info[96];
    snprintf(
        info,
        sizeof(info),
        "PROGRESS %u%%   TEMP %u C   LEVEL %u/5",
        progress,
        temp_c,
        tested_opps);
    flip_draw_text(
        drm,
        centred_text_x(info, 1),
        365,
        info,
        1,
        FG);

    char time_left[64];
    snprintf(
        time_left,
        sizeof(time_left),
        "TIME LEFT ~%u:%02u",
        remaining / 60U,
        remaining % 60U);
    flip_draw_text(
        drm,
        centred_text_x(time_left, 1),
        390,
        time_left,
        1,
        DIM);

    flip_draw_text(
        drm,
        centred_text_x("KEEP CHARGER CONNECTED", 2),
        420,
        "KEEP CHARGER CONNECTED",
        2,
        FG);

    flip_draw_text(
        drm,
        centred_text_x("UNPLUG CHARGER TO CANCEL SAFELY", 1),
        454,
        "UNPLUG CHARGER TO CANCEL SAFELY",
        1,
        DIM);

    flip_drm_present(drm);
}

int main(void)
{
    signal(SIGINT, stop_handler);
    signal(SIGTERM, stop_handler);

    flip_drm_t drm;
    if (flip_drm_open(&drm) < 0)
        return 1;

    unsigned duration_minutes = read_duration_minutes();
    int input_fd = open(
        "/dev/input/event0",
        O_RDONLY | O_NONBLOCK | O_CLOEXEC);

    cpu_sample_t previous[4] = {{0}};
    cpu_sample_t current[4] = {{0}};
    (void)read_cpu_samples(previous);

    unsigned smoothed[4] = {0, 0, 0, 0};

    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);

    while (running) {
        poll_power_key(input_fd);

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        unsigned elapsed =
            (unsigned)(now.tv_sec - start.tv_sec);

        unsigned instantaneous[4] = {0, 0, 0, 0};
        if (read_cpu_samples(current) == 0) {
            for (int i = 0; i < 4; ++i) {
                instantaneous[i] =
                    cpu_usage_percent(&previous[i], &current[i]);
                smoothed[i] =
                    (smoothed[i] * 3U + instantaneous[i]) / 4U;
                previous[i] = current[i];
            }
        }

        char status[80];
        unsigned tested_opps = 0;
        read_validation_status(
            status,
            sizeof(status),
            &tested_opps);

        draw_screen(
            &drm,
            duration_minutes,
            elapsed,
            smoothed,
            friendly_phase(status),
            tested_opps,
            read_soc_temp_c());

        struct timespec delay = {
            .tv_sec = 0,
            .tv_nsec = 100 * 1000 * 1000,
        };
        nanosleep(&delay, NULL);
    }

    if (input_fd >= 0)
        close(input_fd);
    flip_drm_close(&drm);
    return 0;
}
