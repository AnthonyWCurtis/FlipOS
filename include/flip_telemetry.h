#ifndef FLIP_TELEMETRY_H
#define FLIP_TELEMETRY_H

#include <stdint.h>

#define FLIP_TELEMETRY_OVERLAY_LINES 5
#define FLIP_TELEMETRY_OVERLAY_LINE_LENGTH 32

typedef struct {
    double fps;
    int fps_valid;

    double cpu_percent;
    int cpu_percent_valid;
    unsigned long long cpu_khz;
    int cpu_khz_valid;

    double gpu_percent;
    int gpu_percent_valid;
    unsigned long long gpu_hz;
    int gpu_hz_valid;

    long temp_millic;
    int temp_valid;

    int battery_percent;
    int battery_valid;
} flip_telemetry_snapshot_t;

typedef struct {
    uint64_t previous_cpu_total;
    uint64_t previous_cpu_idle;
    int cpu_baseline_valid;

    char cpu_freq_paths[4][256];
    int cpu_freq_path_count;

    char gpu_freq_path[256];
    char gpu_load_path[256];
    char temp_path[256];
    char battery_path[256];

    flip_telemetry_snapshot_t snapshot;
} flip_telemetry_t;

int flip_telemetry_cpu_percent(
    uint64_t previous_total,
    uint64_t previous_idle,
    uint64_t current_total,
    uint64_t current_idle,
    double *percent);

int flip_telemetry_init(
    flip_telemetry_t *telemetry);

int flip_telemetry_sample(
    flip_telemetry_t *telemetry,
    double fps,
    int fps_valid);

const flip_telemetry_snapshot_t *
flip_telemetry_snapshot(
    const flip_telemetry_t *telemetry);

void flip_telemetry_format_overlay(
    const flip_telemetry_snapshot_t *snapshot,
    char lines[FLIP_TELEMETRY_OVERLAY_LINES]
              [FLIP_TELEMETRY_OVERLAY_LINE_LENGTH]);

#endif
