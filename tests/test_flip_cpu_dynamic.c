#include "flip_cpu_dynamic.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
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

static void consume_warmup(flip_cpu_dynamic_t *controller)
{
    for (unsigned i = 0; i < FLIP_CPU_DYNAMIC_WARMUP_WINDOWS; ++i) {
        assert(flip_cpu_dynamic_sample(
            controller, 59.728, 59.728, 10000, 0) == 0);
    }
}

static void test_healthy_workload_stays_put(void)
{
    reset_policy(1104000);

    flip_cpu_dynamic_t controller;
    assert(flip_cpu_dynamic_init(&controller, 1992000) == 1);
    consume_warmup(&controller);

    for (int i = 0; i < 20; ++i) {
        assert(flip_cpu_dynamic_sample(
            &controller, 59.72, 59.728, 11000, 0) == 0);
    }

    assert(read_unsigned_value("scaling_max_freq") == 1104000);
    assert(controller.raise_count == 0);
}

static void test_full_speed_near_budget_does_not_raise(void)
{
    reset_policy(1104000);

    flip_cpu_dynamic_t controller;
    assert(flip_cpu_dynamic_init(&controller, 1992000) == 1);
    consume_warmup(&controller);

    for (int i = 0; i < 8; ++i) {
        assert(flip_cpu_dynamic_sample(
            &controller, 59.976, 59.728, 16626, 0) == 0);
    }

    assert(read_unsigned_value("scaling_max_freq") == 1104000);
    assert(controller.raise_count == 0);
}

static void test_ffvi_style_816_rescue_raises_to_1104(void)
{
    reset_policy(816000);

    flip_cpu_dynamic_t controller;
    assert(flip_cpu_dynamic_init(&controller, 1992000) == 1);
    consume_warmup(&controller);

    assert(controller.current_max_khz == 816000);

    assert(flip_cpu_dynamic_sample(
        &controller, 53.8, 59.728, 19000, 0) == 1);

    assert(read_unsigned_value("scaling_max_freq") == 1104000);
    assert(controller.current_max_khz == 1104000);
    assert(controller.raise_count == 1);
}

static void test_single_timing_wobble_does_not_raise(void)
{
    reset_policy(1104000);

    flip_cpu_dynamic_t controller;
    assert(flip_cpu_dynamic_init(&controller, 1992000) == 1);
    consume_warmup(&controller);

    assert(flip_cpu_dynamic_sample(
        &controller, 59.35, 59.728, 15000, 0) == 0);

    assert(flip_cpu_dynamic_sample(
        &controller, 59.73, 59.728, 12000, 0) == 0);

    assert(read_unsigned_value("scaling_max_freq") == 1104000);
    assert(controller.raise_count == 0);
}

static void test_two_bad_windows_raise_one_opp(void)
{
    reset_policy(1104000);

    flip_cpu_dynamic_t controller;
    assert(flip_cpu_dynamic_init(&controller, 1992000) == 1);
    consume_warmup(&controller);

    assert(flip_cpu_dynamic_sample(
        &controller, 59.20, 59.728, 16000, 0) == 0);

    assert(flip_cpu_dynamic_sample(
        &controller, 59.18, 59.728, 16000, 0) == 1);

    assert(read_unsigned_value("scaling_max_freq") == 1416000);
    assert(controller.current_max_khz == 1416000);
    assert(controller.raise_count == 1);
}

static void test_severe_slowdown_raises_immediately_one_opp(void)
{
    reset_policy(1104000);

    flip_cpu_dynamic_t controller;
    assert(flip_cpu_dynamic_init(&controller, 1992000) == 1);
    consume_warmup(&controller);

    assert(flip_cpu_dynamic_sample(
        &controller, 53.5, 59.728, 18500, 0) == 1);

    assert(read_unsigned_value("scaling_max_freq") == 1416000);
    assert(controller.raise_count == 1);
}

static void test_audio_drop_raises_immediately(void)
{
    reset_policy(1104000);

    flip_cpu_dynamic_t controller;
    assert(flip_cpu_dynamic_init(&controller, 1992000) == 1);
    consume_warmup(&controller);

    assert(flip_cpu_dynamic_sample(
        &controller, 59.72, 59.728, 12000, 1) == 1);

    assert(read_unsigned_value("scaling_max_freq") == 1416000);
}

static void test_allowed_max_is_never_exceeded(void)
{
    reset_policy(1104000);

    flip_cpu_dynamic_t controller;
    assert(flip_cpu_dynamic_init(&controller, 1416000) == 1);
    consume_warmup(&controller);

    assert(flip_cpu_dynamic_sample(
        &controller, 53.0, 59.728, 19000, 0) == 1);

    assert(read_unsigned_value("scaling_max_freq") == 1416000);
    assert(controller.enabled == 1);

    assert(flip_cpu_dynamic_sample(
        &controller, 40.0, 59.728, 25000, 5) == 0);
    assert(read_unsigned_value("scaling_max_freq") == 1416000);
}

static void test_temporary_escalation_is_reclaimed(void)
{
    reset_policy(1104000);

    flip_cpu_dynamic_t controller;
    assert(flip_cpu_dynamic_init(&controller, 1992000) == 1);
    consume_warmup(&controller);

    assert(flip_cpu_dynamic_sample(
        &controller, 53.0, 59.728, 19000, 0) == 1);
    assert(read_unsigned_value("scaling_max_freq") == 1416000);

    for (unsigned i = 0; i < FLIP_CPU_DYNAMIC_GOOD_WINDOWS_TO_LOWER - 1; ++i) {
        assert(flip_cpu_dynamic_sample(
            &controller, 59.728, 59.728, 7000, 0) == 0);
    }
    assert(read_unsigned_value("scaling_max_freq") == 1416000);

    assert(flip_cpu_dynamic_sample(
        &controller, 59.728, 59.728, 7000, 0) == 2);
    assert(read_unsigned_value("scaling_max_freq") == 1104000);
    assert(controller.lower_count == 1);
}

static void test_lowering_never_goes_below_session_base(void)
{
    reset_policy(1104000);

    flip_cpu_dynamic_t controller;
    assert(flip_cpu_dynamic_init(&controller, 1992000) == 1);
    consume_warmup(&controller);

    assert(flip_cpu_dynamic_sample(
        &controller, 53.0, 59.728, 19000, 0) == 1);
    for (unsigned i = 0; i < FLIP_CPU_DYNAMIC_GOOD_WINDOWS_TO_LOWER; ++i)
        (void)flip_cpu_dynamic_sample(&controller, 59.728, 59.728, 7000, 0);

    assert(read_unsigned_value("scaling_max_freq") == 1104000);
    for (unsigned i = 0; i < FLIP_CPU_DYNAMIC_GOOD_WINDOWS_TO_LOWER * 2U; ++i)
        assert(flip_cpu_dynamic_sample(
            &controller, 59.728, 59.728, 7000, 0) == 0);
    assert(read_unsigned_value("scaling_max_freq") == 1104000);
}

static void test_failed_lower_probe_learns_session_floor(void)
{
    reset_policy(408000);

    flip_cpu_dynamic_t controller;
    assert(flip_cpu_dynamic_init(&controller, 1992000) == 1);
    consume_warmup(&controller);

    /* Temporary pressure raises 408 -> 600. */
    assert(flip_cpu_dynamic_sample(
        &controller, 50.0, 59.728, 20000, 0) == 1);
    assert(read_unsigned_value("scaling_max_freq") == 600000);

    /* Healthy operation eventually probes back down to 408. */
    for (unsigned i = 0; i < FLIP_CPU_DYNAMIC_GOOD_WINDOWS_TO_LOWER; ++i)
        (void)flip_cpu_dynamic_sample(&controller, 59.728, 59.728, 7000, 0);
    assert(read_unsigned_value("scaling_max_freq") == 408000);

    /* Immediate regression means 408 was too low: return to 600 and learn it. */
    assert(flip_cpu_dynamic_sample(
        &controller, 50.0, 59.728, 20000, 0) == 1);
    assert(read_unsigned_value("scaling_max_freq") == 600000);
    assert(controller.frequencies_khz[controller.base_index] == 600000);

    /* Do not keep oscillating back to the proven-bad lower ceiling. */
    for (unsigned i = 0; i < FLIP_CPU_DYNAMIC_GOOD_WINDOWS_TO_LOWER * 2U; ++i)
        assert(flip_cpu_dynamic_sample(
            &controller, 59.728, 59.728, 7000, 0) == 0);
    assert(read_unsigned_value("scaling_max_freq") == 600000);
}

static void test_controller_does_not_lower_policy(void)
{
    reset_policy(1608000);

    flip_cpu_dynamic_t controller;
    assert(flip_cpu_dynamic_init(&controller, 1992000) == 1);
    consume_warmup(&controller);

    for (int i = 0; i < 60; ++i) {
        assert(flip_cpu_dynamic_sample(
            &controller, 59.728, 59.728, 7000, 0) == 0);
    }

    assert(read_unsigned_value("scaling_max_freq") == 1608000);
}

int main(void)
{
    test_healthy_workload_stays_put();
    test_full_speed_near_budget_does_not_raise();
    test_ffvi_style_816_rescue_raises_to_1104();
    test_single_timing_wobble_does_not_raise();
    test_two_bad_windows_raise_one_opp();
    test_severe_slowdown_raises_immediately_one_opp();
    test_audio_drop_raises_immediately();
    test_allowed_max_is_never_exceeded();
    test_temporary_escalation_is_reclaimed();
    test_lowering_never_goes_below_session_base();
    test_failed_lower_probe_learns_session_floor();
    test_controller_does_not_lower_policy();

    puts("flip_cpu_dynamic tests passed");
    return EXIT_SUCCESS;
}
