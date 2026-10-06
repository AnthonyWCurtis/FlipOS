#include "flip_cpu_policy.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void write_value(const char *name, const char *value)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", FLIP_CPU_POLICY_ROOT, name);

    FILE *f = fopen(path, "w");
    assert(f != NULL);
    assert(fputs(value, f) >= 0);
    fclose(f);
}

static unsigned read_unsigned_value(const char *name)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", FLIP_CPU_POLICY_ROOT, name);

    FILE *f = fopen(path, "r");
    assert(f != NULL);

    unsigned value = 0;
    assert(fscanf(f, "%u", &value) == 1);
    fclose(f);
    return value;
}

static void read_text_value(const char *name, char *buffer, size_t size)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", FLIP_CPU_POLICY_ROOT, name);

    FILE *f = fopen(path, "r");
    assert(f != NULL);
    assert(fgets(buffer, (int)size, f) != NULL);
    fclose(f);

    char *end = strpbrk(buffer, "\r\n");
    if (end)
        *end = '\0';
}

static void reset_policy(void)
{
    mkdir(FLIP_CPU_POLICY_ROOT, 0700);

    write_value("scaling_available_frequencies",
                "408000 600000 816000 1104000 1416000 1608000 1800000 1992000\n");
    write_value("scaling_min_freq", "408000\n");
    write_value("scaling_max_freq", "1992000\n");
    write_value("scaling_governor", "ondemand\n");
}

static void test_query_and_snapshot(void)
{
    reset_policy();

    flip_cpu_policy_info_t info;
    assert(flip_cpu_policy_query_info(&info) == 0);
    assert(info.frequency_count == 8);
    assert(info.frequencies_khz[0] == 408000);
    assert(info.frequencies_khz[7] == 1992000);

    flip_cpu_policy_snapshot_t snapshot;
    assert(flip_cpu_policy_snapshot(&snapshot) == 0);
    assert(snapshot.min_khz == 408000);
    assert(snapshot.max_khz == 1992000);
    assert(strcmp(snapshot.governor, "ondemand") == 0);
}

static void test_cap_and_restore(void)
{
    reset_policy();

    flip_cpu_policy_snapshot_t original;
    assert(flip_cpu_policy_snapshot(&original) == 0);

    assert(flip_cpu_policy_set_max(1416000) == 0);
    assert(read_unsigned_value("scaling_min_freq") == 408000);
    assert(read_unsigned_value("scaling_max_freq") == 1416000);

    assert(flip_cpu_policy_restore(&original) == 0);
    assert(read_unsigned_value("scaling_min_freq") == 408000);
    assert(read_unsigned_value("scaling_max_freq") == 1992000);

    char governor[64];
    read_text_value("scaling_governor", governor, sizeof(governor));
    assert(strcmp(governor, "ondemand") == 0);
}

static void test_rejects_invalid_frequency(void)
{
    reset_policy();

    assert(flip_cpu_policy_set_max(1500000) < 0);
    assert(read_unsigned_value("scaling_max_freq") == 1992000);

    assert(flip_cpu_policy_set_limits(1416000, 1104000) < 0);
    assert(read_unsigned_value("scaling_min_freq") == 408000);
    assert(read_unsigned_value("scaling_max_freq") == 1992000);
}

int main(void)
{
    assert(flip_cpu_policy_query_info(NULL) < 0);
    assert(flip_cpu_policy_snapshot(NULL) < 0);
    assert(flip_cpu_policy_restore(NULL) < 0);

    test_query_and_snapshot();
    test_cap_and_restore();
    test_rejects_invalid_frequency();

    printf("flip_cpu_policy tests passed\n");
    return EXIT_SUCCESS;
}
