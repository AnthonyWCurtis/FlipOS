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
 * Initialize dynamic ceiling escalation from the current cpufreq policy.
 * The controller never exceeds allowed_max_khz and never lowers below the
 * session's initial ceiling. Temporary escalation is reclaimed after a long
 * run of healthy full-speed performance.
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
 * Returns:
 *   2 = ceiling lowered by one OPP after sustained healthy performance
 *   1 = ceiling raised by one OPP
 *   0 = unchanged
 *  -1 = policy update failed
 */
int flip_cpu_dynamic_sample(
    flip_cpu_dynamic_t *controller,
    double measured_fps,
    double target_fps,
    uint64_t host_avg_us,
    uint64_t audio_dropped_frames);

#endif
