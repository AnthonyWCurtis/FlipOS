#include "flip_perf_diag.h"

#include <string.h>

static void add_metric(
    flip_perf_metric_t *metric,
    uint64_t us)
{
    metric->sum_us += us;

    if (us > metric->max_us)
        metric->max_us = us;
}

void flip_perf_diag_reset(
    flip_perf_diag_t *diag)
{
    if (!diag)
        return;

    memset(
        diag,
        0,
        sizeof(*diag));
}

void flip_perf_diag_record_frame(
    flip_perf_diag_t *diag,
    uint64_t input_us,
    uint64_t controls_us,
    uint64_t core_input_us,
    uint64_t core_run_us,
    uint64_t present_us,
    uint64_t audio_us,
    uint64_t host_us,
    uint64_t late_us,
    int resynced)
{
    if (!diag)
        return;

    diag->frames++;

    add_metric(&diag->input, input_us);
    add_metric(&diag->controls, controls_us);
    add_metric(&diag->core_input, core_input_us);
    add_metric(&diag->core_run, core_run_us);
    add_metric(&diag->present, present_us);
    add_metric(&diag->audio, audio_us);
    add_metric(&diag->host, host_us);

    if (late_us > 0) {
        diag->late_frames++;
        add_metric(&diag->late, late_us);
    }

    if (resynced)
        diag->resyncs++;
}

uint64_t flip_perf_metric_avg_us(
    const flip_perf_metric_t *metric,
    uint64_t frames)
{
    if (!metric ||
        frames == 0)
        return 0;

    return metric->sum_us /
        frames;
}
