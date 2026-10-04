#ifndef FLIP_PERFORMANCE_H
#define FLIP_PERFORMANCE_H

#include <stddef.h>

#ifndef FLIP_PERF_CPU_POLICY_ROOT
#define FLIP_PERF_CPU_POLICY_ROOT \
    "/sys/devices/system/cpu/cpufreq/policy0"
#endif

#ifndef FLIP_PERF_GPU_ROOT
#define FLIP_PERF_GPU_ROOT \
    "/sys/class/devfreq/fde60000.gpu"
#endif

#define FLIP_PERF_GOVERNOR_MAX 64

typedef enum {
    FLIP_PERFORMANCE_BALANCED = 0,
    FLIP_PERFORMANCE_PERFORMANCE,
    FLIP_PERFORMANCE_POWERSAVE
} flip_performance_profile_t;

typedef struct {
    char cpu_governor[FLIP_PERF_GOVERNOR_MAX];
    char gpu_governor[FLIP_PERF_GOVERNOR_MAX];
} flip_performance_snapshot_t;

const char *flip_performance_profile_name(
    flip_performance_profile_t profile);

int flip_performance_query(
    flip_performance_snapshot_t *snapshot);

int flip_performance_profile_supported(
    flip_performance_profile_t profile);

int flip_performance_apply(
    flip_performance_profile_t profile);

int flip_performance_restore(
    const flip_performance_snapshot_t *snapshot);

#endif
