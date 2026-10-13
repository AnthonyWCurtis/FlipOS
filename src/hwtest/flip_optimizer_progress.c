#define _POSIX_C_SOURCE 200809L
#include "flip_drm.h"
#include "flip_draw.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define BG    0x00101010
#define FG    0x00ffffff
#define DIM   0x00505050
#define PANEL 0x00202020

#define CALIBRATION_FILE \
    "/mnt/SDCARD/flipos-dev/state/optimizer/cpu-calibration"
#define SESSION_FILE \
    "/mnt/SDCARD/flipos-dev/state/optimizer/auto-session"
#define RAM_PROGRESS_FILE \
    "/tmp/flipos-optimizer-progress-state"
#define MIN_TUNABLE_KHZ 408000U
#define MAX_OPPS 16U

typedef struct {
    unsigned khz;
    unsigned stock_uv;
    unsigned floor_uv;
    unsigned lowest_stable_uv;
    unsigned highest_unstable_uv;
    unsigned tests_run;
    int complete;
} progress_opp_t;

typedef struct {
    unsigned step_uv;
    unsigned opp_count;
    int pending;
    unsigned pending_index;
    unsigned pending_uv;
    progress_opp_t opps[MAX_OPPS];
} calibration_progress_t;

typedef struct {
    char phase[32];
    unsigned frequency_khz;
    unsigned candidate_uv;
    unsigned duration_seconds;
    unsigned long long started_ms;
} ram_progress_t;

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

static unsigned long long monotonic_ms(void)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
        return 0;
    return (unsigned long long)now.tv_sec * 1000ULL +
        (unsigned long long)now.tv_nsec / 1000000ULL;
}

static int read_calibration(calibration_progress_t *state)
{
    if (!state)
        return -1;

    memset(state, 0, sizeof(*state));
    state->step_uv = 12500U;

    FILE *f = fopen(CALIBRATION_FILE, "r");
    if (!f)
        return -1;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        unsigned index = 0;
        unsigned value = 0;

        if (sscanf(line, "voltage_step_uv=%u", &value) == 1) {
            state->step_uv = value;
            continue;
        }
        if (sscanf(line, "opp_count=%u", &value) == 1) {
            state->opp_count = value > MAX_OPPS ? MAX_OPPS : value;
            continue;
        }
        if (sscanf(line, "pending=%u", &value) == 1) {
            state->pending = value ? 1 : 0;
            continue;
        }
        if (sscanf(line, "pending_index=%u", &value) == 1) {
            state->pending_index = value;
            continue;
        }
        if (sscanf(line, "pending_uv=%u", &value) == 1) {
            state->pending_uv = value;
            continue;
        }
        if (sscanf(line, "opp%u_khz=%u", &index, &value) == 2 &&
            index < MAX_OPPS) {
            state->opps[index].khz = value;
            continue;
        }
        if (sscanf(line, "opp%u_stock_uv=%u", &index, &value) == 2 &&
            index < MAX_OPPS) {
            state->opps[index].stock_uv = value;
            continue;
        }
        if (sscanf(line, "opp%u_floor_uv=%u", &index, &value) == 2 &&
            index < MAX_OPPS) {
            state->opps[index].floor_uv = value;
            continue;
        }
        if (sscanf(line, "opp%u_lowest_stable_uv=%u", &index, &value) == 2 &&
            index < MAX_OPPS) {
            state->opps[index].lowest_stable_uv = value;
            continue;
        }
        if (sscanf(line, "opp%u_highest_unstable_uv=%u", &index, &value) == 2 &&
            index < MAX_OPPS) {
            state->opps[index].highest_unstable_uv = value;
            continue;
        }
        if (sscanf(line, "opp%u_tests_run=%u", &index, &value) == 2 &&
            index < MAX_OPPS) {
            state->opps[index].tests_run = value;
            continue;
        }
        if (sscanf(line, "opp%u_complete=%u", &index, &value) == 2 &&
            index < MAX_OPPS) {
            state->opps[index].complete = value ? 1 : 0;
            continue;
        }
    }

    int failed = ferror(f);
    fclose(f);
    return failed ? -1 : 0;
}

static unsigned read_session_duration(void)
{
    FILE *f = fopen(SESSION_FILE, "r");
    if (!f)
        return 20U;

    char line[128];
    unsigned duration = 20U;
    while (fgets(line, sizeof(line), f)) {
        unsigned value = 0;
        if (sscanf(line, "duration_seconds=%u", &value) == 1 &&
            value >= 5U && value <= 120U) {
            duration = value;
            break;
        }
    }

    fclose(f);
    return duration;
}

static int read_ram_progress(ram_progress_t *state)
{
    if (!state)
        return -1;

    memset(state, 0, sizeof(*state));

    FILE *f = fopen(RAM_PROGRESS_FILE, "r");
    if (!f)
        return -1;

    char line[128];
    while (fgets(line, sizeof(line), f)) {
        char text[32];
        unsigned value = 0;
        unsigned long long wide = 0;

        if (sscanf(line, "phase=%31s", text) == 1) {
            snprintf(state->phase, sizeof(state->phase), "%s", text);
            continue;
        }
        if (sscanf(line, "frequency_khz=%u", &value) == 1) {
            state->frequency_khz = value;
            continue;
        }
        if (sscanf(line, "candidate_uv=%u", &value) == 1) {
            state->candidate_uv = value;
            continue;
        }
        if (sscanf(line, "duration_seconds=%u", &value) == 1) {
            state->duration_seconds = value;
            continue;
        }
        if (sscanf(line, "started_ms=%llu", &wide) == 1) {
            state->started_ms = wide;
            continue;
        }
    }

    int failed = ferror(f);
    fclose(f);
    return failed || !state->phase[0] ? -1 : 0;
}

static unsigned opp_progress_permille(
    const progress_opp_t *opp,
    unsigned step_uv)
{
    if (!opp || !opp->khz)
        return 0;
    if (opp->complete)
        return 1000U;
    if (!opp->stock_uv || !opp->floor_uv ||
        opp->stock_uv <= opp->floor_uv ||
        !opp->lowest_stable_uv)
        return 0;

    unsigned span = opp->stock_uv - opp->floor_uv;
    unsigned unresolved = 0;

    if (opp->highest_unstable_uv &&
        opp->lowest_stable_uv > opp->highest_unstable_uv) {
        unresolved =
            opp->lowest_stable_uv - opp->highest_unstable_uv;
    } else if (opp->lowest_stable_uv > opp->floor_uv) {
        unresolved =
            opp->lowest_stable_uv - opp->floor_uv;
    }

    if (unresolved <= step_uv && opp->highest_unstable_uv)
        return 1000U;
    if (unresolved >= span)
        return 0;

    return (unsigned)(
        ((unsigned long long)(span - unresolved) * 1000ULL) / span);
}

static void summarize(
    const calibration_progress_t *state,
    unsigned *completed,
    unsigned *total,
    unsigned *tests,
    unsigned *overall_permille,
    unsigned *next_khz)
{
    *completed = 0;
    *total = 0;
    *tests = 0;
    *overall_permille = 0;
    *next_khz = 0;

    unsigned long long progress_sum = 0;

    for (unsigned i = 0; i < state->opp_count && i < MAX_OPPS; ++i) {
        const progress_opp_t *opp = &state->opps[i];
        if (opp->khz < MIN_TUNABLE_KHZ)
            continue;

        (*total)++;
        *tests += opp->tests_run;
        progress_sum += opp_progress_permille(opp, state->step_uv);

        if (opp->complete)
            (*completed)++;
    }

    for (unsigned reverse = state->opp_count; reverse > 0; --reverse) {
        const progress_opp_t *opp = &state->opps[reverse - 1];
        if (opp->khz >= MIN_TUNABLE_KHZ && !opp->complete) {
            *next_khz = opp->khz;
            break;
        }
    }

    if (*total)
        *overall_permille = (unsigned)(progress_sum / *total);
}

static unsigned estimate_trials(
    const calibration_progress_t *state)
{
    unsigned trials = 0;
    unsigned step = state->step_uv ? state->step_uv : 12500U;

    for (unsigned i = 0; i < state->opp_count && i < MAX_OPPS; ++i) {
        const progress_opp_t *opp = &state->opps[i];
        if (opp->khz < MIN_TUNABLE_KHZ || opp->complete)
            continue;

        if (!opp->lowest_stable_uv ||
            opp->lowest_stable_uv <= opp->floor_uv) {
            trials += 1U;
            continue;
        }

        if (opp->highest_unstable_uv) {
            unsigned gap =
                opp->lowest_stable_uv - opp->highest_unstable_uv;
            unsigned steps = gap / step;
            unsigned refine = 0;
            while (steps > 1U) {
                steps = (steps + 1U) / 2U;
                refine++;
            }
            trials += refine ? refine : 1U;
            continue;
        }

        unsigned remaining_steps =
            (opp->lowest_stable_uv - opp->floor_uv) / step;

        if (remaining_steps <= 1U) {
            trials += 1U;
        } else {
            /*
             * Before a failure boundary exists, assume it lies roughly
             * halfway through the unexplored range. This is deliberately an
             * estimate; the UI labels the resulting time as EST.
             */
            unsigned expected_coarse =
                (remaining_steps + 3U) / 4U;
            trials += expected_coarse + 1U;
        }
    }

    return trials;
}

static const char *phase_label(const char *phase)
{
    if (!phase || !phase[0])
        return "SELECTING NEXT VOLTAGE";
    if (strcmp(phase, "prepare") == 0)
        return "PREPARING TEST";
    if (strcmp(phase, "parking") == 0)
        return "PARKING CPU SAFELY";
    if (strcmp(phase, "apply") == 0)
        return "APPLYING TEST VOLTAGE";
    if (strcmp(phase, "recovery") == 0)
        return "ARMING RECOVERY";
    if (strcmp(phase, "enter") == 0)
        return "ENTERING PERFORMANCE LEVEL";
    if (strcmp(phase, "integrity-pre") == 0)
        return "CHECKING CPU / MEMORY";
    if (strcmp(phase, "stress") == 0)
        return "STRESS TEST";
    if (strcmp(phase, "integrity-post") == 0)
        return "VERIFYING RESULT";
    if (strcmp(phase, "restore") == 0)
        return "RESTORING STOCK VOLTAGE";
    if (strcmp(phase, "settle") == 0)
        return "SAVING RESULT / RESTARTING";
    return "OPTIMIZER WORKING";
}

static void draw_segmented_bar(
    flip_drm_t *drm,
    int x,
    int y,
    int w,
    int h,
    unsigned total,
    unsigned overall_permille)
{
    if (!total)
        total = 5U;

    flip_draw_rect(drm, x, y, w, h, PANEL);
    flip_draw_rect_outline(drm, x, y, w, h, 2, FG);

    int inner_x = x + 5;
    int inner_y = y + 5;
    int inner_w = w - 10;
    int inner_h = h - 10;
    int gap = 5;
    int seg_w = (inner_w - gap * ((int)total - 1)) / (int)total;

    unsigned scaled =
        overall_permille > 1000U ? 1000U : overall_permille;
    unsigned per_segment = 1000U / total;

    for (unsigned i = 0; i < total; ++i) {
        int sx = inner_x + (seg_w + gap) * (int)i;
        unsigned start = i * per_segment;
        unsigned end = (i + 1U == total)
            ? 1000U
            : (i + 1U) * per_segment;
        unsigned fill_permille = 0;

        if (scaled >= end)
            fill_permille = 1000U;
        else if (scaled > start && end > start)
            fill_permille =
                (scaled - start) * 1000U / (end - start);

        int fill_w =
            (int)((unsigned long long)seg_w * fill_permille / 1000ULL);

        if (fill_w > 0)
            flip_draw_rect(drm, sx, inner_y, fill_w, inner_h, FG);

        flip_draw_rect_outline(
            drm, sx, inner_y, seg_w, inner_h, 1, DIM);
    }
}

static void draw_screen(
    flip_drm_t *drm,
    const calibration_progress_t *calibration,
    const ram_progress_t *ram,
    unsigned default_duration)
{
    unsigned completed = 0;
    unsigned total = 0;
    unsigned tests = 0;
    unsigned overall_permille = 0;
    unsigned next_khz = 0;

    summarize(
        calibration,
        &completed,
        &total,
        &tests,
        &overall_permille,
        &next_khz);

    unsigned frequency_khz =
        ram->frequency_khz ? ram->frequency_khz :
        calibration->pending && calibration->pending_index < MAX_OPPS
            ? calibration->opps[calibration->pending_index].khz
            : next_khz;
    unsigned candidate_uv =
        ram->candidate_uv ? ram->candidate_uv :
        calibration->pending ? calibration->pending_uv : 0;
    unsigned duration =
        ram->duration_seconds ? ram->duration_seconds : default_duration;

    flip_draw_clear(drm, BG);

    flip_draw_text(
        drm, centred_text_x("OPTIMIZING CPU", 4), 26,
        "OPTIMIZING CPU", 4, FG);
    flip_draw_text(
        drm, centred_text_x("DO NOT POWER OFF", 1), 72,
        "DO NOT POWER OFF", 1, DIM);

    char text[128];
    unsigned percent = (overall_permille + 5U) / 10U;
    snprintf(text, sizeof(text), "%u%%", percent > 100U ? 100U : percent);
    flip_draw_text(
        drm, centred_text_x(text, 3), 96, text, 3, FG);

    draw_segmented_bar(drm, 70, 130, 500, 34, total, overall_permille);

    unsigned level = completed < total ? completed + 1U : total;
    snprintf(
        text, sizeof(text),
        "PERFORMANCE LEVEL %u OF %u",
        level ? level : 1U,
        total ? total : 5U);
    flip_draw_text(
        drm, centred_text_x(text, 1), 175, text, 1, DIM);

    if (frequency_khz) {
        if (candidate_uv) {
            snprintf(
                text, sizeof(text),
                "%.3f GHZ   %.4f VOLTS",
                (double)frequency_khz / 1000000.0,
                (double)candidate_uv / 1000000.0);
        } else {
            snprintf(
                text, sizeof(text),
                "%.3f GHZ   SELECTING VOLTAGE",
                (double)frequency_khz / 1000000.0);
        }
        flip_draw_text(
            drm, centred_text_x(text, 2), 201, text, 2, FG);
    }

    const char *stage = phase_label(ram->phase);
    flip_draw_text(
        drm, centred_text_x(stage, 1), 245, stage, 1, FG);

    if (strcmp(ram->phase, "stress") == 0 && duration > 0) {
        unsigned elapsed = 0;
        unsigned long long now_ms = monotonic_ms();
        if (ram->started_ms && now_ms > ram->started_ms)
            elapsed = (unsigned)((now_ms - ram->started_ms) / 1000ULL);
        if (elapsed > duration)
            elapsed = duration;

        const int x = 120;
        const int y = 271;
        const int w = 400;
        const int h = 22;
        flip_draw_rect(drm, x, y, w, h, PANEL);
        flip_draw_rect_outline(drm, x, y, w, h, 1, FG);
        int fill = (int)((unsigned long long)(w - 6) * elapsed / duration);
        if (fill > 0)
            flip_draw_rect(drm, x + 3, y + 3, fill, h - 6, FG);

        snprintf(
            text, sizeof(text),
            "%u / %u SECONDS",
            elapsed, duration);
        flip_draw_text(
            drm, centred_text_x(text, 1), 300, text, 1, DIM);
    } else {
        flip_draw_text(
            drm,
            centred_text_x("RECOVERY PROTECTION ACTIVE", 1),
            281,
            "RECOVERY PROTECTION ACTIVE",
            1,
            DIM);
    }

    snprintf(
        text, sizeof(text),
        "%u VOLTAGE TEST%s COMPLETED",
        tests,
        tests == 1U ? "" : "S");
    flip_draw_text(
        drm, centred_text_x(text, 1), 327, text, 1, DIM);

    unsigned trials = estimate_trials(calibration);
    if (trials > 0) {
        unsigned per_trial = duration + 30U;
        unsigned estimate_sec = trials * per_trial;
        unsigned low_min =
            (estimate_sec * 3U / 4U + 59U) / 60U;
        unsigned high_min =
            (estimate_sec * 3U / 2U + 59U) / 60U;
        if (low_min < 1U)
            low_min = 1U;
        if (high_min < low_min)
            high_min = low_min;

        snprintf(
            text, sizeof(text),
            "EST. %u-%u MIN REMAINING",
            low_min, high_min);
    } else {
        snprintf(text, sizeof(text), "FINALIZING OPTIMIZED PROFILE");
    }
    flip_draw_text(
        drm, centred_text_x(text, 1), 351, text, 1, FG);

    flip_draw_text(
        drm,
        centred_text_x("KEEP CHARGER CONNECTED", 2),
        386,
        "KEEP CHARGER CONNECTED",
        2,
        FG);
    flip_draw_text(
        drm,
        centred_text_x("THE FLIP MAY RESTART SEVERAL TIMES", 1),
        420,
        "THE FLIP MAY RESTART SEVERAL TIMES",
        1,
        DIM);
    flip_draw_text(
        drm,
        centred_text_x("PROGRESS IS SAVED AUTOMATICALLY", 1),
        443,
        "PROGRESS IS SAVED AUTOMATICALLY",
        1,
        DIM);

    flip_drm_present(drm);
}

int main(void)
{
    signal(SIGINT, stop_handler);
    signal(SIGTERM, stop_handler);

    calibration_progress_t calibration;
    if (read_calibration(&calibration) < 0)
        memset(&calibration, 0, sizeof(calibration));

    unsigned default_duration = read_session_duration();

    flip_drm_t drm;
    if (flip_drm_open(&drm) < 0)
        return 1;

    while (running) {
        ram_progress_t ram;
        if (read_ram_progress(&ram) < 0)
            memset(&ram, 0, sizeof(ram));

        draw_screen(
            &drm,
            &calibration,
            &ram,
            default_duration);

        struct timespec delay = {
            .tv_sec = 0,
            .tv_nsec = 200 * 1000 * 1000,
        };
        nanosleep(&delay, NULL);
    }

    flip_drm_close(&drm);
    return 0;
}
