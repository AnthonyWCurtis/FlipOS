#ifndef FLIP_CPU_DYNAMIC_H
#define FLIP_CPU_DYNAMIC_H

#include "flip_cpu_policy.h"

#include <stdint.h>

#define FLIP_CPU_DYNAMIC_WARMUP_WINDOWS 3U
#define FLIP_CPU_DYNAMIC_BAD_WINDOWS_TO_RAISE 2U

typedef struct {
    int enabled;
    unsigned allowed_max_khz;
    unsigned current_max_khz;

    size_t frequency_count;
    unsigned frequencies_khz[FLIP_CPU_POLICY_MAX_FREQS];
    size_t current_index;
    size_t allowed_index;

    unsigned warmup_windows;
    unsigned bad_windows;
    unsigned cooldown_windows;
    unsigned raise_count;
} flip_cpu_dynamic_t;

/*
 * Initialize dynamic ceiling escalation from the current cpufreq policy.
 * The controller never exceeds allowed_max_khz and never lowers the current
 * ceiling. The kernel governor remains responsible for normal down-clocking.
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
