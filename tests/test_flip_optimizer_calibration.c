#include "flip_optimizer_calibration.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const flip_optimizer_calibration_stock_opp_t stock_opps[] = {
    {408000, 850000, 812500},
    {1104000, 850000, 812500},
    {1416000, 925000, 875000},
    {1992000, 1100000, 1037500},
};

static flip_optimizer_calibration_t make_calibration(void)
{
    flip_optimizer_calibration_t calibration;

    assert(flip_optimizer_calibration_init(
               &calibration,
               "b9f4ffca2f298ad2",
               "MIYOO RK3566 355 V10 Board",
               stock_opps,
               sizeof(stock_opps) / sizeof(stock_opps[0]),
               12500,
               25000) == 0);

    return calibration;
}

static flip_optimizer_calibration_t make_wide_calibration(void)
{
    static const flip_optimizer_calibration_stock_opp_t wide[] = {
        {1800000, 1050000, 812500},
    };

    flip_optimizer_calibration_t calibration;

    assert(flip_optimizer_calibration_init(
               &calibration,
               "b9f4ffca2f298ad2",
               "MIYOO RK3566 355 V10 Board",
               wide,
               1,
               12500,
               25000) == 0);

    return calibration;
}

static void test_highest_opp_first_uses_single_step_then_coarse_descent(void)
{
    flip_optimizer_calibration_t calibration =
        make_calibration();

    size_t index = 0;
    unsigned uv = 0;

    assert(flip_optimizer_calibration_prepare_next(
               &calibration,
               &index,
               &uv) == 1);
    assert(index == 3);
    assert(uv == 1087500);
    assert(strcmp(
               calibration.pending_trial_id,
               "cpu-1992000-1087500-1") == 0);

    assert(flip_optimizer_calibration_record_pending(
               &calibration,
               1) == 0);
    assert(calibration.opps[3].lowest_stable_uv == 1087500);
    assert(calibration.opps[3].complete == 0);

    assert(flip_optimizer_calibration_prepare_next(
               &calibration,
               &index,
               &uv) == 1);
    assert(index == 3);
    assert(uv == 1062500);
}

static void test_adaptive_bracket_refines_to_base_step(void)
{
    flip_optimizer_calibration_t calibration =
        make_wide_calibration();

    size_t index = 0;
    unsigned uv = 0;

    assert(flip_optimizer_calibration_prepare_next(
               &calibration, &index, &uv) == 1);
    assert(index == 0);
    assert(uv == 1037500);
    assert(flip_optimizer_calibration_record_pending(
               &calibration, 1) == 0);

    assert(flip_optimizer_calibration_prepare_next(
               &calibration, &index, &uv) == 1);
    assert(uv == 1012500);
    assert(flip_optimizer_calibration_record_pending(
               &calibration, 0) == 0);

    const flip_optimizer_calibration_opp_t *opp =
        &calibration.opps[0];

    assert(opp->lowest_stable_uv == 1037500);
    assert(opp->highest_unstable_uv == 1012500);
    assert(opp->complete == 0);

    assert(flip_optimizer_calibration_prepare_next(
               &calibration, &index, &uv) == 1);
    assert(uv == 1025000);
    assert(flip_optimizer_calibration_record_pending(
               &calibration, 1) == 0);

    opp = &calibration.opps[0];
    assert(opp->lowest_stable_uv == 1025000);
    assert(opp->highest_unstable_uv == 1012500);
    assert(opp->raw_final_uv == 1050000);
    assert(opp->tests_run == 3);
    assert(opp->complete == 1);
}

static void test_guardband_is_above_lowest_proven_stable(void)
{
    flip_optimizer_calibration_t calibration =
        make_wide_calibration();

    size_t index = 0;
    unsigned uv = 0;

    assert(flip_optimizer_calibration_prepare_next(
               &calibration, &index, &uv) == 1);
    assert(uv == 1037500);
    assert(flip_optimizer_calibration_record_pending(
               &calibration, 1) == 0);

    assert(flip_optimizer_calibration_prepare_next(
               &calibration, &index, &uv) == 1);
    assert(uv == 1012500);
    assert(flip_optimizer_calibration_record_pending(
               &calibration, 0) == 0);

    assert(flip_optimizer_calibration_prepare_next(
               &calibration, &index, &uv) == 1);
    assert(uv == 1025000);
    assert(flip_optimizer_calibration_record_pending(
               &calibration, 0) == 0);

    const flip_optimizer_calibration_opp_t *opp =
        &calibration.opps[0];

    assert(opp->lowest_stable_uv == 1037500);
    assert(opp->highest_unstable_uv == 1025000);
    assert(opp->raw_final_uv == 1050000);
    assert(opp->complete == 1);
}

static void test_rollback_creates_refinement_bracket(void)
{
    flip_optimizer_calibration_t calibration =
        make_wide_calibration();

    size_t index = 0;
    unsigned uv = 0;

    assert(flip_optimizer_calibration_prepare_next(
               &calibration,
               &index,
               &uv) == 1);
    assert(uv == 1037500);

    char trial_id[
        FLIP_OPTIMIZER_CALIBRATION_TRIAL_ID_MAX + 1];
    strcpy(trial_id, calibration.pending_trial_id);

    assert(flip_optimizer_calibration_record_rollback(
               &calibration,
               "wrong-trial") < 0);

    assert(flip_optimizer_calibration_record_rollback(
               &calibration,
               trial_id) == 0);

    assert(calibration.pending == 0);
    assert(calibration.opps[0].complete == 1);
    assert(calibration.opps[0].highest_unstable_uv == 1037500);
    assert(calibration.opps[0].lowest_stable_uv == 1050000);
    assert(calibration.opps[0].raw_final_uv == 1050000);

    assert(flip_optimizer_calibration_prepare_next(
               &calibration,
               &index,
               &uv) == 0);
}

static void test_hard_rollback_finalizes_without_more_danger_probes(void)
{
    flip_optimizer_calibration_t calibration =
        make_wide_calibration();

    size_t index = 0;
    unsigned uv = 0;

    assert(flip_optimizer_calibration_prepare_next(
               &calibration, &index, &uv) == 1);
    assert(uv == 1037500);
    assert(flip_optimizer_calibration_record_pending(
               &calibration, 1) == 0);

    assert(flip_optimizer_calibration_prepare_next(
               &calibration, &index, &uv) == 1);
    assert(uv == 1012500);

    char trial_id[
        FLIP_OPTIMIZER_CALIBRATION_TRIAL_ID_MAX + 1];
    strcpy(trial_id, calibration.pending_trial_id);

    assert(flip_optimizer_calibration_record_rollback(
               &calibration, trial_id) == 0);
    assert(calibration.opps[0].lowest_stable_uv == 1037500);
    assert(calibration.opps[0].highest_unstable_uv == 1012500);
    assert(calibration.opps[0].complete == 0);

    assert(flip_optimizer_calibration_finalize_after_hard_rollback(
               &calibration, 0) == 0);

    assert(calibration.opps[0].complete == 1);
    assert(calibration.opps[0].raw_final_uv == 1050000);
    assert(calibration.opps[0].tests_run == 2);

    assert(flip_optimizer_calibration_prepare_next(
               &calibration, &index, &uv) == 0);
}

static void test_floor_completion(void)
{
    flip_optimizer_calibration_t calibration =
        make_calibration();

    for (size_t i = 1; i < calibration.opp_count; ++i) {
        calibration.opps[i].complete = 1;
        calibration.opps[i].raw_final_uv =
            calibration.opps[i].stock_uv;
        calibration.opps[i].tests_run = 1;
    }

    size_t index = 0;
    unsigned uv = 0;

    assert(flip_optimizer_calibration_prepare_next(
               &calibration,
               &index,
               &uv) == 1);
    assert(index == 0);
    assert(uv == 837500);

    assert(flip_optimizer_calibration_record_pending(
               &calibration,
               1) == 0);

    assert(flip_optimizer_calibration_prepare_next(
               &calibration,
               &index,
               &uv) == 1);
    assert(index == 0);
    assert(uv == 812500);

    assert(flip_optimizer_calibration_record_pending(
               &calibration,
               1) == 0);

    assert(calibration.opps[0].complete == 1);
    assert(calibration.opps[0].lowest_stable_uv == 812500);
    assert(calibration.opps[0].raw_final_uv == 837500);
}

static void test_monotonic_result(void)
{
    flip_optimizer_calibration_t calibration =
        make_calibration();

    for (size_t i = 0; i < calibration.opp_count; ++i) {
        calibration.opps[i].complete = 1;
        calibration.opps[i].tests_run = 1;
    }

    calibration.opps[0].raw_final_uv = 837500;
    calibration.opps[1].raw_final_uv = 825000;
    calibration.opps[2].raw_final_uv = 900000;
    calibration.opps[3].raw_final_uv = 1062500;

    assert(flip_optimizer_calibration_validate(
               &calibration) == 0);
    assert(flip_optimizer_calibration_is_complete(
               &calibration) == 1);

    flip_optimizer_result_t result;
    assert(flip_optimizer_calibration_build_result(
               &calibration,
               1234,
               &result) == 0);

    assert(result.opps[0].optimized_uv == 837500);
    assert(result.opps[1].optimized_uv == 837500);
    assert(result.opps[2].optimized_uv == 900000);
    assert(result.opps[3].optimized_uv == 1062500);
    assert(result.guardband_uv == 25000);
}

static void test_preserve_low_opps_at_stock(void)
{
    flip_optimizer_calibration_t calibration =
        make_calibration();

    assert(flip_optimizer_calibration_preserve_stock_below(
               &calibration,
               1104000) == 0);

    assert(calibration.opps[0].frequency_khz == 408000);
    assert(calibration.opps[0].complete == 1);
    assert(calibration.opps[0].lowest_stable_uv ==
           calibration.opps[0].stock_uv);
    assert(calibration.opps[0].raw_final_uv ==
           calibration.opps[0].stock_uv);
    assert(calibration.opps[0].highest_unstable_uv == 0);
    assert(calibration.opps[0].tests_run == 0);

    assert(calibration.opps[1].frequency_khz == 1104000);
    assert(calibration.opps[1].complete == 0);
    assert(calibration.opps[1].raw_final_uv == 0);

    size_t index = 0;
    unsigned uv = 0;
    assert(flip_optimizer_calibration_prepare_next(
               &calibration,
               &index,
               &uv) == 1);

    assert(flip_optimizer_calibration_preserve_stock_below(
               &calibration,
               1104000) < 0);
}

static void test_save_load_pending_state(void)
{
    assert(system(
        "rm -rf " FLIP_OPTIMIZER_CALIBRATION_ROOT) == 0);

    flip_optimizer_calibration_t calibration =
        make_calibration();

    size_t index = 0;
    unsigned uv = 0;
    assert(flip_optimizer_calibration_prepare_next(
               &calibration,
               &index,
               &uv) == 1);
    assert(flip_optimizer_calibration_save(
               &calibration) == 0);

    flip_optimizer_calibration_t loaded;
    assert(flip_optimizer_calibration_load(
               &loaded) == 0);

    assert(strcmp(
               loaded.device_id,
               calibration.device_id) == 0);
    assert(loaded.pending == 1);
    assert(loaded.pending_index == index);
    assert(loaded.pending_uv == uv);
    assert(strcmp(
               loaded.pending_trial_id,
               calibration.pending_trial_id) == 0);
}

int main(void)
{
    test_highest_opp_first_uses_single_step_then_coarse_descent();
    test_adaptive_bracket_refines_to_base_step();
    test_guardband_is_above_lowest_proven_stable();
    test_rollback_creates_refinement_bracket();
    test_hard_rollback_finalizes_without_more_danger_probes();
    test_floor_completion();
    test_monotonic_result();
    test_preserve_low_opps_at_stock();
    test_save_load_pending_state();

    printf("flip_optimizer_calibration tests passed\n");
    return EXIT_SUCCESS;
}
