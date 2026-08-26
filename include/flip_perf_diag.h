#ifndef FLIP_PERF_DIAG_H
#define FLIP_PERF_DIAG_H

#include <stdint.h>

typedef struct {
    uint64_t sum_us;
    uint64_t max_us;
} flip_perf_metric_t;

typedef struct {
    uint64_t frames;

    flip_perf_metric_t input;
    flip_perf_metric_t controls;
    flip_perf_metric_t core_input;
    flip_perf_metric_t core_run;
    flip_perf_metric_t present;
    flip_perf_metric_t audio;
    flip_perf_metric_t host;
    flip_perf_metric_t late;

    uint64_t late_frames;
    uint64_t resyncs;
} flip_perf_diag_t;

void flip_perf_diag_reset(
    flip_perf_diag_t *diag);

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
    int resynced);

uint64_t flip_perf_metric_avg_us(
    const flip_perf_metric_t *metric,
    uint64_t frames);

#endif
