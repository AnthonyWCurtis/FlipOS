#ifndef FLIP_CPU_DYNAMIC_H
#define FLIP_CPU_DYNAMIC_H

#include "flip_cpu_policy.h"

#include <stdint.h>

#define FLIP_CPU_DYNAMIC_WARMUP_WINDOWS 3U
#define FLIP_CPU_DYNAMIC_BAD_WINDOWS_TO_RAISE 2U
#define FLIP_CPU_DYNAMIC_GOOD_WINDOWS_TO_LOWER 15U
#define FLIP_CPU_DYNAMIC_LOWER_PROBE_WINDOWS 3U

typedef struct {
    int enabled;
    unsigned allowed_max_khz;
    unsigned current_max_khz;

    size_t frequency_count;
    unsigned frequencies_khz[FLIP_CPU_POLICY_MAX_FREQS];
    size_t current_index;
    size_t allowed_index;
    size_t base_index;

    unsigned warmup_windows;
    unsigned bad_windows;
    unsigned good_windows;
    unsigned cooldown_windows;
    unsigned lower_probe_windows;
    unsigned raise_count;
    unsigned lower_count;
} flip_cpu_dynamic_t;

/*
 * Initialize dynamic OPP control from the current cpufreq policy.
 * While active, the controller keeps scaling_min_freq and scaling_max_freq
 * together at the selected OPP so the ondemand governor cannot dive below the
 * proven performance floor between emulation frames. The controller never
 * exceeds allowed_max_khz and never lowers below the session's initial OPP.
 * Temporary escalation is reclaimed after a long run of healthy full-speed
 * performance.
 *
 * Returns:
 *   1 = dynamic escalation available
 *   0 = already at/above allowed maximum, nothing to manage
 *  -1 = invalid or unavailable policy
 */
int flip_cpu_dynamic_init(
    flip_cpu_dynamic_t *controller,
    unsigned allowed_max_khz);

/*
 * Feed one performance window, normally about one second.
 *
 * Average FPS is not enough to detect every visible hitch. late_max_us and
 * timing_resyncs carry frame-pacing burst evidence from the shared host so a
 * short cluster of missed deadlines can request more headroom even when the
 * one-second FPS average still looks healthy.
 *
 * Returns:
 *   2 = selected OPP lowered by one step after sustained healthy performance
 *   1 = selected OPP raised by one step
 *   0 = unchanged
 *  -1 = policy update failed
 */
int flip_cpu_dynamic_sample(
    flip_cpu_dynamic_t *controller,
    double measured_fps,
    double target_fps,
    uint64_t host_avg_us,
    uint64_t late_max_us,
    uint64_t timing_resyncs,
    uint64_t audio_dropped_frames);

#endif
