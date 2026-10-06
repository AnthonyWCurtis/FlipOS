#include "flip_cpu_profile.h"

#include <string.h>

int flip_cpu_profile_apply(
    const char *system_id,
    flip_cpu_profile_t *profile)
{
    if (!system_id || !profile)
        return -1;

    memset(profile, 0, sizeof(*profile));

    if (strcmp(system_id, "GBA") != 0)
        return 0;

    if (flip_cpu_policy_snapshot(&profile->original) < 0)
        return -1;

    /*
     * Never raise an already-lower ceiling. A thermal, battery or future
     * safety policy must always win over the performance profile.
     */
    if (profile->original.max_khz <= FLIP_CPU_PROFILE_GBA_MAX_KHZ)
        return 0;

    if (flip_cpu_policy_set_max(FLIP_CPU_PROFILE_GBA_MAX_KHZ) < 0)
        return -1;

    profile->active = 1;
    profile->applied_max_khz = FLIP_CPU_PROFILE_GBA_MAX_KHZ;
    return 1;
}

int flip_cpu_profile_restore(
    flip_cpu_profile_t *profile)
{
    if (!profile)
        return -1;

    if (!profile->active)
        return 0;

    if (flip_cpu_policy_restore(&profile->original) < 0)
        return -1;

    profile->active = 0;
    profile->applied_max_khz = 0;
    return 0;
}
