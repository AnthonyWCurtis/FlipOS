#include "flip_performance.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void make_dir(
    const char *path)
{
    mkdir(path, 0700);
}

static void write_file(
    const char *root,
    const char *name,
    const char *value)
{
    char path[512];

    snprintf(
        path,
        sizeof(path),
        "%s/%s",
        root,
        name);

    FILE *f = fopen(path, "w");
    assert(f != NULL);
    assert(fputs(value, f) >= 0);
    fclose(f);
}

static void read_file(
    const char *root,
    const char *name,
    char *buffer,
    size_t buffer_size)
{
    char path[512];

    snprintf(
        path,
        sizeof(path),
        "%s/%s",
        root,
        name);

    FILE *f = fopen(path, "r");
    assert(f != NULL);
    assert(fgets(
        buffer,
        (int)buffer_size,
        f) != NULL);
    fclose(f);

    char *end =
        strpbrk(buffer, "\r\n");

    if (end)
        *end = '\0';
}

static void reset_roots(void)
{
    make_dir("/tmp/flipos-performance-test");
    make_dir(FLIP_PERF_CPU_POLICY_ROOT);
    make_dir(FLIP_PERF_GPU_ROOT);

    write_file(
        FLIP_PERF_CPU_POLICY_ROOT,
        "scaling_available_governors",
        "ondemand powersave performance schedutil\n");

    write_file(
        FLIP_PERF_GPU_ROOT,
        "available_governors",
        "simple_ondemand powersave performance\n");

    write_file(
        FLIP_PERF_CPU_POLICY_ROOT,
        "scaling_governor",
        "ondemand\n");

    write_file(
        FLIP_PERF_GPU_ROOT,
        "governor",
        "simple_ondemand\n");
}

static void test_query_and_names(void)
{
    reset_roots();

    flip_performance_snapshot_t snapshot;

    assert(flip_performance_query(
        &snapshot) == 0);

    assert(strcmp(
        snapshot.cpu_governor,
        "ondemand") == 0);

    assert(strcmp(
        snapshot.gpu_governor,
        "simple_ondemand") == 0);

    assert(strcmp(
        flip_performance_profile_name(
            FLIP_PERFORMANCE_BALANCED),
        "BALANCED") == 0);

    assert(strcmp(
        flip_performance_profile_name(
            FLIP_PERFORMANCE_PERFORMANCE),
        "PERFORMANCE") == 0);

    assert(strcmp(
        flip_performance_profile_name(
            FLIP_PERFORMANCE_POWERSAVE),
        "POWERSAVE") == 0);
}

static void test_apply_profiles_and_restore(void)
{
    reset_roots();

    flip_performance_snapshot_t original;

    assert(flip_performance_query(
        &original) == 0);

    assert(flip_performance_profile_supported(
        FLIP_PERFORMANCE_BALANCED) == 1);
    assert(flip_performance_profile_supported(
        FLIP_PERFORMANCE_PERFORMANCE) == 1);
    assert(flip_performance_profile_supported(
        FLIP_PERFORMANCE_POWERSAVE) == 1);

    assert(flip_performance_apply(
        FLIP_PERFORMANCE_PERFORMANCE) == 0);

    char value[64];

    read_file(
        FLIP_PERF_CPU_POLICY_ROOT,
        "scaling_governor",
        value,
        sizeof(value));
    assert(strcmp(value, "performance") == 0);

    read_file(
        FLIP_PERF_GPU_ROOT,
        "governor",
        value,
        sizeof(value));
    assert(strcmp(value, "performance") == 0);

    assert(flip_performance_apply(
        FLIP_PERFORMANCE_POWERSAVE) == 0);

    read_file(
        FLIP_PERF_CPU_POLICY_ROOT,
        "scaling_governor",
        value,
        sizeof(value));
    assert(strcmp(value, "powersave") == 0);

    read_file(
        FLIP_PERF_GPU_ROOT,
        "governor",
        value,
        sizeof(value));
    assert(strcmp(value, "powersave") == 0);

    assert(flip_performance_restore(
        &original) == 0);

    read_file(
        FLIP_PERF_CPU_POLICY_ROOT,
        "scaling_governor",
        value,
        sizeof(value));
    assert(strcmp(value, "ondemand") == 0);

    read_file(
        FLIP_PERF_GPU_ROOT,
        "governor",
        value,
        sizeof(value));
    assert(strcmp(value, "simple_ondemand") == 0);
}

static void test_unsupported_is_non_destructive(void)
{
    reset_roots();

    write_file(
        FLIP_PERF_GPU_ROOT,
        "available_governors",
        "simple_ondemand powersave\n");

    assert(flip_performance_profile_supported(
        FLIP_PERFORMANCE_PERFORMANCE) == 0);

    assert(flip_performance_apply(
        FLIP_PERFORMANCE_PERFORMANCE) == 1);

    char value[64];

    read_file(
        FLIP_PERF_CPU_POLICY_ROOT,
        "scaling_governor",
        value,
        sizeof(value));
    assert(strcmp(value, "ondemand") == 0);

    read_file(
        FLIP_PERF_GPU_ROOT,
        "governor",
        value,
        sizeof(value));
    assert(strcmp(value, "simple_ondemand") == 0);
}

static void test_partial_apply_rolls_back_cpu(void)
{
    reset_roots();

    char gpu_path[512];

    snprintf(
        gpu_path,
        sizeof(gpu_path),
        "%s/governor",
        FLIP_PERF_GPU_ROOT);

    assert(chmod(gpu_path, 0400) == 0);

    assert(flip_performance_apply(
        FLIP_PERFORMANCE_PERFORMANCE) < 0);

    char value[64];

    read_file(
        FLIP_PERF_CPU_POLICY_ROOT,
        "scaling_governor",
        value,
        sizeof(value));

    assert(strcmp(value, "ondemand") == 0);

    assert(chmod(gpu_path, 0600) == 0);
}

int main(void)
{
    assert(flip_performance_query(NULL) < 0);
    assert(flip_performance_apply(
        (flip_performance_profile_t)99) < 0);
    assert(flip_performance_restore(NULL) < 0);

    test_query_and_names();
    test_apply_profiles_and_restore();
    test_unsupported_is_non_destructive();
    test_partial_apply_rolls_back_cpu();

    printf("flip_performance tests passed\n");
    return EXIT_SUCCESS;
}
