#define _POSIX_C_SOURCE 200809L

#include "flip_draw.h"

#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define BG    0x00101010
#define FG    0x00ffffff
#define DIM   0x00505050
#define PANEL 0x00202020

#define VALIDATION_REQUEST \
    "/mnt/SDCARD/flipos-dev/state/optimizer/validation-request"
#define VALIDATION_ACTIVE \
    "/mnt/SDCARD/flipos-dev/state/optimizer/validation-active"
#define VALIDATION_MANUAL_STOP \
    "/mnt/SDCARD/flipos-dev/state/optimizer/validation-manual-stop"

#define VALIDATION_LEVELS 8U

static const unsigned validation_frequencies_khz[VALIDATION_LEVELS] = {
    1992000U,
    1800000U,
    1608000U,
    1416000U,
    1104000U,
    816000U,
    600000U,
    408000U,
};

static volatile sig_atomic_t running = 1;

typedef struct {
    unsigned long long total;
    unsigned long long idle;
} cpu_sample_t;

typedef struct {
    char status[80];
    char phase[32];
    unsigned tested_opps;
    unsigned seconds_per_opp;
    unsigned max_temp_mc;
    unsigned frequency_khz;
    unsigned voltage_uv;
    unsigned chunk_index;
    unsigned chunk_count;
    unsigned recovery_armed;
    unsigned tested_epoch;
} validation_status_t;

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
            if (ev.type == EV_KEY &&
                ev.code == KEY_POWER &&
                ev.value == 1)
                record_manual_stop();
            continue;
        }

        if (n < 0 &&
            (errno == EAGAIN ||
             errno == EWOULDBLOCK ||
             errno == EINTR))
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
            if (parsed == 5U ||
                parsed == 10U ||
                parsed == 20U ||
                parsed == 30U ||
                parsed == 60U)
                minutes = parsed;
            break;
        }
    }

    fclose(f);
    return minutes;
}

static void read_validation_status(validation_status_t *state)
{
    if (!state)
        return;

    memset(state, 0, sizeof(*state));
    snprintf(state->status, sizeof(state->status), "PREPARING");

    FILE *f = fopen(VALIDATION_ACTIVE, "r");
    if (!f)
        return;

    char line[192];

    while (fgets(line, sizeof(line), f)) {
        char text[80];
        unsigned parsed = 0;

        if (sscanf(line, "status=%79s", text) == 1) {
            snprintf(
                state->status,
                sizeof(state->status),
                "%s",
                text);
            continue;
        }

        if (sscanf(line, "phase=%31s", text) == 1) {
            snprintf(
                state->phase,
                sizeof(state->phase),
                "%.31s",
                text);
            continue;
        }

        if (sscanf(line, "tested_opps=%u", &parsed) == 1) {
            state->tested_opps = parsed;
            continue;
        }

        if (sscanf(line, "seconds_per_opp=%u", &parsed) == 1) {
            state->seconds_per_opp = parsed;
            continue;
        }

        if (sscanf(line, "max_temp_mc=%u", &parsed) == 1) {
            state->max_temp_mc = parsed;
            continue;
        }

        if (sscanf(line, "frequency_khz=%u", &parsed) == 1) {
            state->frequency_khz = parsed;
            continue;
        }

        if (sscanf(line, "voltage_uv=%u", &parsed) == 1) {
            state->voltage_uv = parsed;
            continue;
        }

        if (sscanf(line, "chunk_index=%u", &parsed) == 1) {
            state->chunk_index = parsed;
            continue;
        }

        if (sscanf(line, "chunk_count=%u", &parsed) == 1) {
            state->chunk_count = parsed;
            continue;
        }

        if (sscanf(line, "recovery_armed=%u", &parsed) == 1) {
            state->recovery_armed = parsed;
            continue;
        }

        if (sscanf(line, "tested_epoch=%u", &parsed) == 1) {
            state->tested_epoch = parsed;
            continue;
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
                user + nice + system + idle + iowait +
                irq + softirq + steal;
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
    if (!before ||
        !after ||
        after->total <= before->total)
        return 0;

    unsigned long long total =
        after->total - before->total;
    unsigned long long idle =
        after->idle >= before->idle
            ? after->idle - before->idle
            : 0;

    if (idle >= total)
        return 0;

    return (unsigned)(
        ((total - idle) * 100ULL) / total);
}

static unsigned read_soc_temp_c(void)
{
    FILE *f =
        fopen("/sys/class/thermal/thermal_zone0/temp", "r");

    if (!f)
        return 0;

    unsigned temp_mc = 0;
    if (fscanf(f, "%u", &temp_mc) != 1)
        temp_mc = 0;

    fclose(f);
    return (temp_mc + 500U) / 1000U;
}

static const char *friendly_phase(
    const validation_status_t *state)
{
    if (!state || !state->status[0])
        return "PREPARING";

    if (strncmp(state->status, "entering-", 9) == 0)
        return "STRESSING ALL 4 CPU CORES";

    if (strcmp(state->status, "running") == 0)
        return "PREPARING";

    if (strcmp(state->status, "arming-quiesce") == 0)
        return "PREPARING SYSTEM";

    if (strcmp(state->status, "arming-panic-policy") == 0 ||
        strcmp(state->status, "loading-tsadc-recovery") == 0 ||
        strcmp(state->status, "arming-tsadc-recovery") == 0 ||
        strcmp(state->status, "arming-hw-watchdog") == 0 ||
        strcmp(state->status, "arming-deadman") == 0)
        return "ARMING RECOVERY PROTECTION";

    if (strcmp(state->status, "applying-profile") == 0)
        return "APPLYING TUNED SETTINGS";

    if (strcmp(state->status, "profile-applied") == 0)
        return "STARTING CPU LOAD";

    return "STABILITY TEST RUNNING";
}

static unsigned current_chunk_seconds(
    const validation_status_t *state)
{
    if (!state ||
        !state->seconds_per_opp ||
        !state->chunk_count)
        return 0;

    unsigned seconds =
        state->seconds_per_opp / state->chunk_count;

    return seconds ? seconds : 1U;
}

static unsigned current_chunk_elapsed(
    const validation_status_t *state)
{
    unsigned chunk_seconds =
        current_chunk_seconds(state);

    if (!state ||
        !chunk_seconds ||
        !state->tested_epoch)
        return 0;

    time_t now = time(NULL);
    if (now <= 0 ||
        (unsigned long long)now <= state->tested_epoch)
        return 0;

    unsigned elapsed =
        (unsigned)((unsigned long long)now -
                   state->tested_epoch);

    return elapsed > chunk_seconds
        ? chunk_seconds
        : elapsed;
}

static unsigned stress_elapsed_seconds(
    const validation_status_t *state)
{
    if (!state || !state->seconds_per_opp)
        return 0;

    unsigned elapsed =
        state->tested_opps * state->seconds_per_opp;

    if (state->frequency_khz &&
        state->chunk_index &&
        state->chunk_count) {
        unsigned chunk_seconds =
            current_chunk_seconds(state);

        elapsed +=
            (state->chunk_index - 1U) *
            chunk_seconds;
        elapsed += current_chunk_elapsed(state);
    }

    return elapsed;
}

static void draw_level_timeline(
    flip_drm_t *drm,
    unsigned tested_opps)
{
    const int box_w = 64;
    const int box_h = 27;
    const int gap = 6;
    const int total_w =
        (int)VALIDATION_LEVELS * box_w +
        ((int)VALIDATION_LEVELS - 1) * gap;
    const int start_x = (640 - total_w) / 2;
    const int y = 126;

    for (unsigned i = 0;
         i < VALIDATION_LEVELS;
         ++i) {
        int x = start_x + (int)i * (box_w + gap);

        int done = i < tested_opps;
        int active =
            i == tested_opps &&
            tested_opps < VALIDATION_LEVELS;

        flip_draw_rect(
            drm,
            x,
            y,
            box_w,
            box_h,
            done ? FG : PANEL);

        flip_draw_rect_outline(
            drm,
            x,
            y,
            box_w,
            box_h,
            active || done ? 2 : 1,
            active || done ? FG : DIM);

        char mhz[16];
        snprintf(
            mhz,
            sizeof(mhz),
            "%u",
            validation_frequencies_khz[i] / 1000U);

        flip_draw_text(
            drm,
            x + (box_w - (int)strlen(mhz) * 6) / 2,
            y + 9,
            mhz,
            1,
            done ? BG : (active ? FG : DIM));

        const char *state =
            done ? "DONE" :
            active ? "TEST" :
            "WAIT";

        flip_draw_text(
            drm,
            x + (box_w - (int)strlen(state) * 6) / 2,
            y + box_h + 7,
            state,
            1,
            done || active ? FG : DIM);
    }
}

static void draw_core_load(
    flip_drm_t *drm,
    const unsigned usage[4])
{
    const int label_x = 65;
    const int bar_x = 105;
    const int bar_w = 385;
    const int bar_h = 15;
    const int percent_x = 510;
    const int first_y = 245;
    const int stride = 24;

    for (int i = 0; i < 4; ++i) {
        int y = first_y + i * stride;
        unsigned clamped =
            usage[i] > 100U ? 100U : usage[i];

        char core[8];
        snprintf(core, sizeof(core), "C%d", i);
        flip_draw_text(
            drm,
            label_x,
            y + 2,
            core,
            1,
            FG);

        flip_draw_rect(
            drm,
            bar_x,
            y,
            bar_w,
            bar_h,
            PANEL);
        flip_draw_rect_outline(
            drm,
            bar_x,
            y,
            bar_w,
            bar_h,
            1,
            FG);

        int fill =
            (int)((unsigned long long)(bar_w - 4) *
                  clamped / 100ULL);

        if (fill > 0) {
            flip_draw_rect(
                drm,
                bar_x + 2,
                y + 2,
                fill,
                bar_h - 4,
                FG);
        }

        char percent[16];
        snprintf(
            percent,
            sizeof(percent),
            "%3u%%",
            clamped);
        flip_draw_text(
            drm,
            percent_x,
            y + 2,
            percent,
            1,
            clamped >= 95U ? FG : DIM);
    }
}

static void draw_screen(
    flip_drm_t *drm,
    unsigned duration_minutes,
    const validation_status_t *state,
    const unsigned usage[4],
    unsigned temp_c,
    unsigned observed_peak_c)
{
    flip_draw_clear(drm, BG);

    flip_draw_text(
        drm,
        centred_text_x("CPU STABILITY TEST", 3),
        17,
        "CPU STABILITY TEST",
        3,
        FG);

    unsigned total_seconds =
        duration_minutes * 60U;

    unsigned stress_elapsed =
        stress_elapsed_seconds(state);

    unsigned remaining =
        stress_elapsed >= total_seconds
            ? 0U
            : total_seconds - stress_elapsed;

    char countdown[32];
    snprintf(
        countdown,
        sizeof(countdown),
        "%02u:%02u",
        remaining / 60U,
        remaining % 60U);

    flip_draw_text(
        drm,
        centred_text_x(countdown, 4),
        57,
        countdown,
        4,
        FG);

    flip_draw_text(
        drm,
        centred_text_x("STRESS TIME REMAINING", 1),
        101,
        "STRESS TIME REMAINING",
        1,
        DIM);

    draw_level_timeline(
        drm,
        state->tested_opps > VALIDATION_LEVELS
            ? VALIDATION_LEVELS
            : state->tested_opps);

    char current[96];

    if (state->frequency_khz && state->voltage_uv) {
        unsigned whole_mv =
            state->voltage_uv / 1000U;
        unsigned tenth_mv =
            (state->voltage_uv % 1000U) / 100U;

        snprintf(
            current,
            sizeof(current),
            "TESTING %u MHz @ %u.%u mV",
            state->frequency_khz / 1000U,
            whole_mv,
            tenth_mv);
    } else {
        snprintf(
            current,
            sizeof(current),
            "%s",
            friendly_phase(state));
    }

    flip_draw_text(
        drm,
        centred_text_x(current, 2),
        183,
        current,
        2,
        FG);

    const char *phase = friendly_phase(state);
    flip_draw_text(
        drm,
        centred_text_x(phase, 1),
        213,
        phase,
        1,
        DIM);

    draw_core_load(drm, usage);

    unsigned chunk_seconds =
        current_chunk_seconds(state);
    unsigned chunk_elapsed =
        current_chunk_elapsed(state);

    if (state->chunk_count &&
        state->chunk_index &&
        chunk_seconds) {
        const int x = 105;
        const int y = 348;
        const int w = 430;
        const int h = 16;

        flip_draw_rect(
            drm,
            x,
            y,
            w,
            h,
            PANEL);
        flip_draw_rect_outline(
            drm,
            x,
            y,
            w,
            h,
            1,
            FG);

        int fill =
            (int)((unsigned long long)(w - 4) *
                  chunk_elapsed /
                  chunk_seconds);

        if (fill > 0) {
            flip_draw_rect(
                drm,
                x + 2,
                y + 2,
                fill,
                h - 4,
                FG);
        }

        char window[96];
        snprintf(
            window,
            sizeof(window),
            "LOAD WINDOW %u/%u   %u/%u SEC",
            state->chunk_index,
            state->chunk_count,
            chunk_elapsed,
            chunk_seconds);

        flip_draw_text(
            drm,
            centred_text_x(window, 1),
            373,
            window,
            1,
            DIM);
    } else {
        flip_draw_text(
            drm,
            centred_text_x(
                "PREPARING VALIDATION LOAD",
                1),
            357,
            "PREPARING VALIDATION LOAD",
            1,
            DIM);
    }

    unsigned recorded_peak_c =
        state->max_temp_mc
            ? (state->max_temp_mc + 500U) / 1000U
            : 0U;

    unsigned peak_c =
        observed_peak_c > recorded_peak_c
            ? observed_peak_c
            : recorded_peak_c;

    char thermal[80];
    snprintf(
        thermal,
        sizeof(thermal),
        "CPU TEMP %u C   PEAK %u C",
        temp_c,
        peak_c);

    flip_draw_text(
        drm,
        centred_text_x(thermal, 1),
        397,
        thermal,
        1,
        FG);

    const char *protection =
        state->recovery_armed
            ? "RECOVERY PROTECTION ACTIVE"
            : "RECOVERY PROTECTION PREPARING";

    flip_draw_text(
        drm,
        centred_text_x(protection, 1),
        418,
        protection,
        1,
        state->recovery_armed ? FG : DIM);

    flip_draw_text(
        drm,
        centred_text_x("KEEP CHARGER CONNECTED", 2),
        439,
        "KEEP CHARGER CONNECTED",
        2,
        FG);

    flip_draw_text(
        drm,
        centred_text_x(
            "UNPLUG CHARGER TO CANCEL SAFELY",
            1),
        468,
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

    unsigned duration_minutes =
        read_duration_minutes();

    int input_fd = open(
        "/dev/input/event0",
        O_RDONLY | O_NONBLOCK | O_CLOEXEC);

    cpu_sample_t previous[4] = {{0}};
    cpu_sample_t current[4] = {{0}};
    (void)read_cpu_samples(previous);

    unsigned smoothed[4] = {0, 0, 0, 0};
    unsigned observed_peak_c = 0;

    while (running) {
        poll_power_key(input_fd);

        unsigned instantaneous[4] = {0, 0, 0, 0};

        if (read_cpu_samples(current) == 0) {
            for (int i = 0; i < 4; ++i) {
                instantaneous[i] =
                    cpu_usage_percent(
                        &previous[i],
                        &current[i]);

                smoothed[i] =
                    (smoothed[i] * 2U +
                     instantaneous[i] * 2U) /
                    4U;

                previous[i] = current[i];
            }
        }

        validation_status_t state;
        read_validation_status(&state);

        unsigned temp_c = read_soc_temp_c();
        if (temp_c > observed_peak_c)
            observed_peak_c = temp_c;

        draw_screen(
            &drm,
            duration_minutes,
            &state,
            smoothed,
            temp_c,
            observed_peak_c);

        struct timespec delay = {
            .tv_sec = 0,
            .tv_nsec = 250 * 1000 * 1000,
        };
        nanosleep(&delay, NULL);
    }

    if (input_fd >= 0)
        close(input_fd);

    flip_drm_close(&drm);
    return 0;
}
