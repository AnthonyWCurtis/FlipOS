#ifndef FLIP_CPU_POLICY_H
#define FLIP_CPU_POLICY_H

#include <stddef.h>

#ifndef FLIP_CPU_POLICY_ROOT
#define FLIP_CPU_POLICY_ROOT "/sys/devices/system/cpu/cpufreq/policy0"
#endif

#define FLIP_CPU_POLICY_MAX_FREQS 16
#define FLIP_CPU_POLICY_GOVERNOR_MAX 64

typedef struct {
    unsigned min_khz;
    unsigned max_khz;
    char governor[FLIP_CPU_POLICY_GOVERNOR_MAX];
} flip_cpu_policy_snapshot_t;

typedef struct {
    size_t frequency_count;
    unsigned frequencies_khz[FLIP_CPU_POLICY_MAX_FREQS];
} flip_cpu_policy_info_t;

int flip_cpu_policy_query_info(flip_cpu_policy_info_t *info);
int flip_cpu_policy_snapshot(flip_cpu_policy_snapshot_t *snapshot);
int flip_cpu_policy_set_limits(unsigned min_khz, unsigned max_khz);
int flip_cpu_policy_set_max(unsigned max_khz);
int flip_cpu_policy_restore(const flip_cpu_policy_snapshot_t *snapshot);

#endif
