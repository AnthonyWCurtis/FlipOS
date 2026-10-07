#include "flip_optimizer_calibration_session.h"

#include "flip_optimizer_guard.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const flip_optimizer_calibration_stock_opp_t stock_opps[] = {
    {408000, 850000, 812500},
    {1416000, 925000, 875000},
    {1992000, 1100000, 1037500},
};

static void reset_roots(void)
{
    assert(system(
        "rm -rf " FLIP_OPTIMIZER_CALIBRATION_ROOT) == 0);
}

static flip_optimizer_calibration_t make_calibration(void)
{
    flip_optimizer_calibration_t calibration;

    assert(flip_optimizer_calibration_init(
               &calibration,
               "device-a",
               "Miyoo Flip",
               stock_opps,
               sizeof(stock_opps) / sizeof(stock_opps[0]),
               12500,
               25000) == 0);

    assert(flip_optimizer_calibration_save(
               &calibration) == 0);

    return calibration;
}

static void test_prepare_arms_matching_guard(void)
{
    reset_roots();
    flip_optimizer_calibration_t calibration =
        make_calibration();

    size_t index = 0;
    unsigned uv = 0;

    assert(flip_optimizer_calibration_session_prepare(
               &calibration,
               &index,
               &uv) == 1);
    assert(index == 2);
    assert(uv == 1087500);
    assert(calibration.pending == 1);
    assert(
        flip_optimizer_calibration_session_ready_for_apply(
            &calibration) == 1);

    flip_optimizer_guard_status_t guard;
    assert(flip_optimizer_guard_status(&guard) == 0);
    assert(guard.active == 1);
    assert(strcmp(
               guard.trial_id,
               calibration.pending_trial_id) == 0);
}

static void test_confirmed_stable_commits_candidate(void)
{
    reset_roots();
    flip_optimizer_calibration_t calibration =
        make_calibration();

    assert(flip_optimizer_calibration_session_prepare(
               &calibration,
               NULL,
               NULL) == 1);

    assert(flip_optimizer_calibration_session_confirm_stable(
               &calibration) == 0);
    assert(calibration.pending == 0);
    assert(calibration.opps[2].lowest_stable_uv == 1087500);

    flip_optimizer_guard_status_t guard;
    assert(flip_optimizer_guard_status(&guard) == 0);
    assert(guard.active == 0);
    assert(guard.stock_only == 0);
    assert(strcmp(guard.last_action, "confirmed") == 0);
}


static void test_controlled_failure_commits_unstable(void)
{
    reset_roots();
    flip_optimizer_calibration_t calibration =
        make_calibration();

    assert(flip_optimizer_calibration_session_prepare(
               &calibration,
               NULL,
               NULL) == 1);

    unsigned candidate = calibration.pending_uv;

    assert(
        flip_optimizer_calibration_session_confirm_unstable(
            &calibration) == 0);

    assert(calibration.pending == 0);
    assert(calibration.opps[2].complete == 1);
    assert(calibration.opps[2].highest_unstable_uv == candidate);
    assert(calibration.opps[2].tests_run == 1);

    flip_optimizer_guard_status_t guard;
    assert(flip_optimizer_guard_status(&guard) == 0);
    assert(guard.active == 0);
    assert(guard.stock_only == 0);
    assert(strcmp(guard.last_action, "failed") == 0);
}

static void test_cancel_unapplied_removes_pending_and_guard(void)
{
    reset_roots();
    flip_optimizer_calibration_t calibration =
        make_calibration();

    assert(flip_optimizer_calibration_session_prepare(
               &calibration,
               NULL,
               NULL) == 1);
    assert(
        flip_optimizer_calibration_session_ready_for_apply(
            &calibration) == 1);

    assert(
        flip_optimizer_calibration_session_cancel_unapplied(
            &calibration) == 0);

    assert(calibration.pending == 0);
    assert(calibration.opps[2].tests_run == 0);

    flip_optimizer_guard_status_t guard;
    assert(flip_optimizer_guard_status(&guard) == 0);
    assert(guard.active == 0);
    assert(guard.stock_only == 0);
    assert(strcmp(guard.last_action, "cancelled") == 0);
}

static void test_reboot_rollback_marks_candidate_unstable(void)
{
    reset_roots();
    flip_optimizer_calibration_t calibration =
        make_calibration();

    assert(flip_optimizer_calibration_session_prepare(
               &calibration,
               NULL,
               NULL) == 1);

    char trial[
        FLIP_OPTIMIZER_CALIBRATION_TRIAL_ID_MAX + 1];
    strcpy(trial, calibration.pending_trial_id);

    assert(flip_optimizer_guard_mark_entering(
               trial) == 0);

    char rolled[
        FLIP_OPTIMIZER_GUARD_TRIAL_ID_MAX + 1];
    assert(flip_optimizer_guard_boot_check(
               100,
               rolled,
               sizeof(rolled)) == 1);
    assert(strcmp(rolled, trial) == 0);

    assert(flip_optimizer_calibration_session_reconcile(
               &calibration) ==
           FLIP_OPTIMIZER_RECONCILE_ROLLED_BACK);

    assert(calibration.pending == 0);
    assert(calibration.opps[2].complete == 1);
    assert(calibration.opps[2].highest_unstable_uv ==
           1087500);

    assert(flip_optimizer_calibration_session_prepare(
               &calibration,
               NULL,
               NULL) < 0);

    assert(
        flip_optimizer_calibration_session_clear_stock_only(
            &calibration) == 0);
}

static void test_confirm_event_recovers_pending_state(void)
{
    reset_roots();
    flip_optimizer_calibration_t calibration =
        make_calibration();

    assert(flip_optimizer_calibration_session_prepare(
               &calibration,
               NULL,
               NULL) == 1);

    char trial[
        FLIP_OPTIMIZER_CALIBRATION_TRIAL_ID_MAX + 1];
    strcpy(trial, calibration.pending_trial_id);

    assert(flip_optimizer_guard_confirm(
               trial,
               200) == 0);

    assert(calibration.pending == 1);

    assert(flip_optimizer_calibration_session_reconcile(
               &calibration) ==
           FLIP_OPTIMIZER_RECONCILE_CONFIRMED);

    assert(calibration.pending == 0);
    assert(calibration.opps[2].lowest_stable_uv == 1087500);
}

static void test_orphaned_prearm_candidate_is_cancelled(void)
{
    reset_roots();
    flip_optimizer_calibration_t calibration =
        make_calibration();

    assert(flip_optimizer_calibration_prepare_next(
               &calibration,
               NULL,
               NULL) == 1);
    assert(flip_optimizer_calibration_save(
               &calibration) == 0);
    assert(calibration.pending == 1);

    assert(flip_optimizer_calibration_session_reconcile(
               &calibration) ==
           FLIP_OPTIMIZER_RECONCILE_ORPHANED);

    assert(calibration.pending == 0);
    assert(calibration.opps[2].tests_run == 0);
}

static void test_boot_recovery_releases_exact_rollback(void)
{
    reset_roots();
    flip_optimizer_calibration_t calibration =
        make_calibration();

    assert(flip_optimizer_calibration_session_prepare(
               &calibration,
               NULL,
               NULL) == 1);

    char trial[
        FLIP_OPTIMIZER_CALIBRATION_TRIAL_ID_MAX + 1];
    strcpy(trial, calibration.pending_trial_id);

    assert(flip_optimizer_guard_mark_entering(
               trial) == 0);

    char rolled[
        FLIP_OPTIMIZER_GUARD_TRIAL_ID_MAX + 1];
    assert(flip_optimizer_guard_boot_check(
               700,
               rolled,
               sizeof(rolled)) == 1);
    assert(strcmp(rolled, trial) == 0);

    int reconcile = -1;
    int released = 0;
    assert(flip_optimizer_calibration_session_recover_boot(
               &calibration,
               &reconcile,
               &released) == 0);
    assert(reconcile ==
           FLIP_OPTIMIZER_RECONCILE_ROLLED_BACK);
    assert(released == 1);
    assert(calibration.pending == 0);

    flip_optimizer_guard_status_t guard;
    assert(flip_optimizer_guard_status(&guard) == 0);
    assert(guard.active == 0);
    assert(guard.stock_only == 0);
    assert(strcmp(
               guard.last_trial_id,
               trial) == 0);
}

static void test_boot_recovery_keeps_ambiguous_quarantine(void)
{
    reset_roots();
    flip_optimizer_calibration_t calibration =
        make_calibration();

    assert(flip_optimizer_calibration_prepare_next(
               &calibration,
               NULL,
               NULL) == 1);
    assert(flip_optimizer_calibration_save(
               &calibration) == 0);

    assert(flip_optimizer_guard_arm(
               "different-trial",
               710) == 0);

    char rolled[
        FLIP_OPTIMIZER_GUARD_TRIAL_ID_MAX + 1];
    assert(flip_optimizer_guard_boot_check(
               711,
               rolled,
               sizeof(rolled)) == 1);
    assert(strcmp(rolled, "different-trial") == 0);

    int reconcile = -1;
    int released = 0;
    assert(flip_optimizer_calibration_session_recover_boot(
               &calibration,
               &reconcile,
               &released) == 0);
    assert(reconcile ==
           FLIP_OPTIMIZER_RECONCILE_ORPHANED);
    assert(released == 0);

    flip_optimizer_guard_status_t guard;
    assert(flip_optimizer_guard_status(&guard) == 0);
    assert(guard.stock_only == 1);
}

int main(void)
{
    test_prepare_arms_matching_guard();
    test_confirmed_stable_commits_candidate();
    test_controlled_failure_commits_unstable();
    test_cancel_unapplied_removes_pending_and_guard();
    test_reboot_rollback_marks_candidate_unstable();
    test_confirm_event_recovers_pending_state();
    test_orphaned_prearm_candidate_is_cancelled();
    test_boot_recovery_releases_exact_rollback();
    test_boot_recovery_keeps_ambiguous_quarantine();

    printf("flip_optimizer_calibration_session tests passed\n");
    return EXIT_SUCCESS;
}
