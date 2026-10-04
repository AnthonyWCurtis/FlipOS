#include "flip_performance.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    const char *cpu_governor;
    const char *gpu_governor;
} flip_performance_definition_t;

static int definition_for(
    flip_performance_profile_t profile,
    flip_performance_definition_t *definition)
{
    if (!definition)
        return -1;

    switch (profile) {
    case FLIP_PERFORMANCE_BALANCED:
        definition->cpu_governor = "ondemand";
        definition->gpu_governor = "simple_ondemand";
        return 0;

    case FLIP_PERFORMANCE_PERFORMANCE:
        definition->cpu_governor = "performance";
        definition->gpu_governor = "performance";
        return 0;

    case FLIP_PERFORMANCE_POWERSAVE:
        definition->cpu_governor = "powersave";
        definition->gpu_governor = "powersave";
        return 0;
    }

    return -1;
}

static int make_path(
    char *path,
    size_t path_size,
    const char *root,
    const char *name)
{
    if (!path ||
        path_size == 0 ||
        !root ||
        !name)
        return -1;

    int written = snprintf(
        path,
        path_size,
        "%s/%s",
        root,
        name);

    if (written < 0 ||
        (size_t)written >= path_size)
        return -1;

    return 0;
}

static int read_line(
    const char *path,
    char *buffer,
    size_t buffer_size)
{
    if (!path ||
        !buffer ||
        buffer_size == 0)
        return -1;

    FILE *f = fopen(path, "r");

    if (!f)
        return -1;

    if (!fgets(
            buffer,
            (int)buffer_size,
            f)) {
        fclose(f);
        return -1;
    }

    fclose(f);

    char *end =
        strpbrk(
            buffer,
            "\r\n");

    if (end)
        *end = '\0';

    return buffer[0]
        ? 0
        : -1;
}

static int write_line(
    const char *path,
    const char *value)
{
    if (!path ||
        !value ||
        !value[0])
        return -1;

    FILE *f = fopen(path, "w");

    if (!f)
        return -1;

    int ok =
        fprintf(
            f,
            "%s\n",
            value);

    if (fflush(f) != 0)
        ok = -1;

    if (fclose(f) != 0)
        ok = -1;

    return ok < 0
        ? -1
        : 0;
}

static int token_present(
    const char *list,
    const char *wanted)
{
    if (!list ||
        !wanted ||
        !wanted[0])
        return 0;

    const char *p = list;

    while (*p) {
        while (*p == ' ' ||
               *p == '\t')
            ++p;

        const char *start = p;

        while (*p &&
               *p != ' ' &&
               *p != '\t')
            ++p;

        size_t length =
            (size_t)(p - start);

        if (length == strlen(wanted) &&
            strncmp(
                start,
                wanted,
                length) == 0)
            return 1;
    }

    return 0;
}

static int governor_paths(
    char *cpu_governor,
    size_t cpu_governor_size,
    char *cpu_available,
    size_t cpu_available_size,
    char *gpu_governor,
    size_t gpu_governor_size,
    char *gpu_available,
    size_t gpu_available_size)
{
    return
        make_path(
            cpu_governor,
            cpu_governor_size,
            FLIP_PERF_CPU_POLICY_ROOT,
            "scaling_governor") == 0 &&
        make_path(
            cpu_available,
            cpu_available_size,
            FLIP_PERF_CPU_POLICY_ROOT,
            "scaling_available_governors") == 0 &&
        make_path(
            gpu_governor,
            gpu_governor_size,
            FLIP_PERF_GPU_ROOT,
            "governor") == 0 &&
        make_path(
            gpu_available,
            gpu_available_size,
            FLIP_PERF_GPU_ROOT,
            "available_governors") == 0
            ? 0
            : -1;
}

const char *flip_performance_profile_name(
    flip_performance_profile_t profile)
{
    switch (profile) {
    case FLIP_PERFORMANCE_BALANCED:
        return "BALANCED";
    case FLIP_PERFORMANCE_PERFORMANCE:
        return "PERFORMANCE";
    case FLIP_PERFORMANCE_POWERSAVE:
        return "POWERSAVE";
    }

    return "UNKNOWN";
}

int flip_performance_query(
    flip_performance_snapshot_t *snapshot)
{
    if (!snapshot)
        return -1;

    memset(
        snapshot,
        0,
        sizeof(*snapshot));

    char cpu_governor[512];
    char cpu_available[512];
    char gpu_governor[512];
    char gpu_available[512];

    if (governor_paths(
            cpu_governor,
            sizeof(cpu_governor),
            cpu_available,
            sizeof(cpu_available),
            gpu_governor,
            sizeof(gpu_governor),
            gpu_available,
            sizeof(gpu_available)) < 0)
        return -1;

    if (read_line(
            cpu_governor,
            snapshot->cpu_governor,
            sizeof(snapshot->cpu_governor)) < 0)
        return -1;

    if (read_line(
            gpu_governor,
            snapshot->gpu_governor,
            sizeof(snapshot->gpu_governor)) < 0)
        return -1;

    return 0;
}

int flip_performance_profile_supported(
    flip_performance_profile_t profile)
{
    flip_performance_definition_t definition;

    if (definition_for(
            profile,
            &definition) < 0)
        return -1;

    char cpu_governor[512];
    char cpu_available_path[512];
    char gpu_governor[512];
    char gpu_available_path[512];

    if (governor_paths(
            cpu_governor,
            sizeof(cpu_governor),
            cpu_available_path,
            sizeof(cpu_available_path),
            gpu_governor,
            sizeof(gpu_governor),
            gpu_available_path,
            sizeof(gpu_available_path)) < 0)
        return -1;

    char cpu_available[512];
    char gpu_available[512];

    if (read_line(
            cpu_available_path,
            cpu_available,
            sizeof(cpu_available)) < 0 ||
        read_line(
            gpu_available_path,
            gpu_available,
            sizeof(gpu_available)) < 0)
        return -1;

    return token_present(
               cpu_available,
               definition.cpu_governor) &&
           token_present(
               gpu_available,
               definition.gpu_governor)
        ? 1
        : 0;
}

int flip_performance_restore(
    const flip_performance_snapshot_t *snapshot)
{
    if (!snapshot ||
        !snapshot->cpu_governor[0] ||
        !snapshot->gpu_governor[0])
        return -1;

    char cpu_governor[512];
    char cpu_available[512];
    char gpu_governor[512];
    char gpu_available[512];

    if (governor_paths(
            cpu_governor,
            sizeof(cpu_governor),
            cpu_available,
            sizeof(cpu_available),
            gpu_governor,
            sizeof(gpu_governor),
            gpu_available,
            sizeof(gpu_available)) < 0)
        return -1;

    if (write_line(
            cpu_governor,
            snapshot->cpu_governor) < 0)
        return -1;

    if (write_line(
            gpu_governor,
            snapshot->gpu_governor) < 0)
        return -1;

    return 0;
}

int flip_performance_apply(
    flip_performance_profile_t profile)
{
    flip_performance_definition_t definition;

    if (definition_for(
            profile,
            &definition) < 0)
        return -1;

    int supported =
        flip_performance_profile_supported(
            profile);

    if (supported != 1)
        return supported == 0
            ? 1
            : -1;

    flip_performance_snapshot_t previous;

    if (flip_performance_query(
            &previous) < 0)
        return -1;

    char cpu_governor[512];
    char cpu_available[512];
    char gpu_governor[512];
    char gpu_available[512];

    if (governor_paths(
            cpu_governor,
            sizeof(cpu_governor),
            cpu_available,
            sizeof(cpu_available),
            gpu_governor,
            sizeof(gpu_governor),
            gpu_available,
            sizeof(gpu_available)) < 0)
        return -1;

    if (strcmp(
            previous.cpu_governor,
            definition.cpu_governor) != 0 &&
        write_line(
            cpu_governor,
            definition.cpu_governor) < 0)
        return -1;

    if (strcmp(
            previous.gpu_governor,
            definition.gpu_governor) != 0 &&
        write_line(
            gpu_governor,
            definition.gpu_governor) < 0) {

        write_line(
            cpu_governor,
            previous.cpu_governor);

        return -1;
    }

    return 0;
}
