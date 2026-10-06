#include "flip_cpu_profile.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static void write_value(const char *name, const char *value)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", FLIP_CPU_POLICY_ROOT, name);

    FILE *f = fopen(path, "w");
    assert(f != NULL);
    assert(fputs(value, f) >= 0);
    assert(fclose(f) == 0);
}

static unsigned read_unsigned_value(const char *name)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", FLIP_CPU_POLICY_ROOT, name);

    FILE *f = fopen(path, "r");
    assert(f != NULL);

    unsigned value = 0;
    assert(fscanf(f, "%u", &value) == 1);
    assert(fclose(f) == 0);
    return value;
}

static void reset_policy(unsigned max_khz)
{
    assert(system("rm -rf " FLIP_CPU_POLICY_ROOT) == 0);
    assert(mkdir(FLIP_CPU_POLICY_ROOT, 0700) == 0);

    write_value(
        "scaling_available_frequencies",
        "408000 600000 816000 1104000 1416000 1608000 1800000 1992000\n");
    write_value("scaling_min_freq", "408000\n");

    char max_text[32];
    snprintf(max_text, sizeof(max_text), "%u\n", max_khz);
    write_value("scaling_max_freq", max_text);
    write_value("scaling_governor", "ondemand\n");
}

static void test_gba_profile_caps_and_restores(void)
{
    reset_policy(1992000);

    flip_cpu_profile_t profile;
    assert(flip_cpu_profile_apply("GBA", &profile) == 1);
    assert(profile.active == 1);
    assert(profile.original.max_khz == 1992000);
    assert(profile.applied_max_khz == 1104000);
    assert(read_unsigned_value("scaling_max_freq") == 1104000);

    assert(flip_cpu_profile_restore(&profile) == 0);
    assert(profile.active == 0);
    assert(read_unsigned_value("scaling_max_freq") == 1992000);
}

static void test_gb_and_gbc_profiles_cap_at_minimum_opp(void)
{
    static const char *systems[] = { "GB", "GBC" };

    for (size_t i = 0; i < sizeof(systems) / sizeof(systems[0]); ++i) {
        reset_policy(1992000);

        flip_cpu_profile_t profile;
        assert(flip_cpu_profile_apply(systems[i], &profile) == 1);
        assert(profile.active == 1);
        assert(profile.original.max_khz == 1992000);
        assert(profile.applied_max_khz == 408000);
        assert(read_unsigned_value("scaling_max_freq") == 408000);

        assert(flip_cpu_profile_restore(&profile) == 0);
        assert(profile.active == 0);
        assert(read_unsigned_value("scaling_max_freq") == 1992000);
    }
}

static void test_gba_profile_never_raises_lower_limit(void)
{
    reset_policy(816000);

    flip_cpu_profile_t profile;
    assert(flip_cpu_profile_apply("GBA", &profile) == 0);
    assert(profile.active == 0);
    assert(read_unsigned_value("scaling_max_freq") == 816000);

    assert(flip_cpu_profile_restore(&profile) == 0);
    assert(read_unsigned_value("scaling_max_freq") == 816000);
}

static void test_other_systems_are_unchanged(void)
{
    reset_policy(1992000);

    flip_cpu_profile_t profile;
    assert(flip_cpu_profile_apply("NES", &profile) == 0);
    assert(profile.active == 0);
    assert(read_unsigned_value("scaling_max_freq") == 1992000);
}

int main(void)
{
    test_gba_profile_caps_and_restores();
    test_gb_and_gbc_profiles_cap_at_minimum_opp();
    test_gba_profile_never_raises_lower_limit();
    test_other_systems_are_unchanged();

    puts("flip_cpu_profile tests passed");
    return EXIT_SUCCESS;
}
