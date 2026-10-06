#ifndef FLIP_CPU_PROFILE_H
#define FLIP_CPU_PROFILE_H

#include "flip_cpu_policy.h"

#define FLIP_CPU_PROFILE_GBA_MAX_KHZ 1104000U

typedef struct {
    int active;
    unsigned applied_max_khz;
    flip_cpu_policy_snapshot_t original;
} flip_cpu_profile_t;

/*
 * Apply the runtime CPU profile for a system.
 *
 * Returns:
 *   1 = profile applied
 *   0 = no profile needed / existing cap is already lower or equal
 *  -1 = profile could not be applied
 */
int flip_cpu_profile_apply(
    const char *system_id,
    flip_cpu_profile_t *profile);

/*
 * Restore the exact policy captured before apply().
 *
 * Returns:
 *   0 = restored or no active profile
 *  -1 = restoration failed
 */
int flip_cpu_profile_restore(
    flip_cpu_profile_t *profile);

#endif
