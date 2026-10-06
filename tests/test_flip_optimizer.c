#include "flip_optimizer.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static flip_optimizer_result_t make_result(void)
{
    flip_optimizer_result_t result;
    memset(&result, 0, sizeof(result));

    result.version = FLIP_OPTIMIZER_VERSION;
    strcpy(result.device_id, "rk3566-test-unit");
    strcpy(result.model, "Miyoo Flip");
    result.calibrated_epoch = 1791300000U;
    result.guardband_uv = 25000;
    result.opp_count = 3;

    result.opps[0] = (flip_optimizer_opp_t){408000, 850000, 825000};
    result.opps[1] = (flip_optimizer_opp_t){1416000, 925000, 900000};
    result.opps[2] = (flip_optimizer_opp_t){1992000, 1100000, 1050000};

    return result;
}

static void test_validation(void)
{
    flip_optimizer_result_t result = make_result();
    assert(flip_optimizer_validate(&result) == 0);

    result.opps[1].optimized_uv = 950000;
    assert(flip_optimizer_validate(&result) < 0);

    result = make_result();
    result.opps[2].frequency_khz = 1104000;
    assert(flip_optimizer_validate(&result) < 0);

    result = make_result();
    result.opps[2].optimized_uv = 875000;
    assert(flip_optimizer_validate(&result) < 0);
}

static void test_summary_without_measured_power(void)
{
    flip_optimizer_result_t result = make_result();
    flip_optimizer_summary_t summary;

    assert(flip_optimizer_summarize(&result, &summary) == 0);
    assert(fabs(summary.top_opp_voltage_reduction_mv - 50.0) < 0.001);
    assert(summary.top_opp_cpu_dynamic_reduction_percent > 8.8);
    assert(summary.top_opp_cpu_dynamic_reduction_percent < 9.0);
    assert(summary.measured_power_valid == 0);
    assert(summary.estimated_extra_runtime_minutes == 0.0);
}

static void test_summary_with_measured_power(void)
{
    flip_optimizer_result_t result = make_result();
    result.power_measurement_valid = 1;
    result.stock_power_mw = 2720.0;
    result.optimized_power_mw = 2500.0;
    result.baseline_runtime_minutes = 300;

    flip_optimizer_summary_t summary;
    assert(flip_optimizer_summarize(&result, &summary) == 0);
    assert(summary.measured_power_valid == 1);
    assert(summary.whole_device_power_reduction_percent > 8.0);
    assert(summary.whole_device_power_reduction_percent < 8.2);
    assert(summary.estimated_runtime_minutes > 326.3);
    assert(summary.estimated_runtime_minutes < 326.5);
    assert(summary.estimated_extra_runtime_minutes > 26.3);
    assert(summary.estimated_extra_runtime_minutes < 26.5);
}

static void test_save_and_load(void)
{
    assert(system("rm -rf " FLIP_OPTIMIZER_ROOT) == 0);

    flip_optimizer_result_t result = make_result();
    result.power_measurement_valid = 1;
    result.stock_power_mw = 2720.0;
    result.optimized_power_mw = 2500.0;
    result.baseline_runtime_minutes = 300;

    assert(flip_optimizer_save(&result) == 0);

    flip_optimizer_result_t loaded;
    assert(flip_optimizer_load(&loaded) == 0);
    assert(strcmp(loaded.device_id, result.device_id) == 0);
    assert(strcmp(loaded.model, result.model) == 0);
    assert(loaded.opp_count == result.opp_count);
    assert(loaded.opps[2].optimized_uv == 1050000);
    assert(loaded.power_measurement_valid == 1);
    assert(fabs(loaded.stock_power_mw - 2720.0) < 0.001);
}

int main(void)
{
    assert(flip_optimizer_validate(NULL) < 0);
    assert(flip_optimizer_load(NULL) < 0);
    assert(flip_optimizer_save(NULL) < 0);
    assert(flip_optimizer_summarize(NULL, NULL) < 0);

    test_validation();
    test_summary_without_measured_power();
    test_summary_with_measured_power();
    test_save_and_load();

    printf("flip_optimizer tests passed\n");
    return EXIT_SUCCESS;
}
