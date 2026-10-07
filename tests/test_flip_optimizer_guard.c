#include "flip_optimizer_guard.h"
#include "flip_optimizer_pmu_token.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void reset_root(void)
{
    assert(system("rm -rf " FLIP_OPTIMIZER_GUARD_ROOT) == 0);
}

static void test_arm_and_confirm(void)
{
    reset_root();

    flip_optimizer_guard_status_t status;
    assert(flip_optimizer_guard_status(&status) == 0);
    assert(status.active == 0);
    assert(status.stock_only == 0);

    assert(flip_optimizer_guard_arm("trial-001", 100) == 0);
    assert(flip_optimizer_guard_arm("trial-002", 101) < 0);

    assert(flip_optimizer_guard_status(&status) == 0);
    assert(status.active == 1);
    assert(status.stock_only == 0);
    assert(strcmp(status.trial_id, "trial-001") == 0);
    assert(status.armed_epoch == 100);
    assert(strcmp(status.last_action, "armed") == 0);

    assert(flip_optimizer_guard_confirm("wrong", 110) < 0);
    assert(flip_optimizer_guard_confirm("trial-001", 111) == 0);

    assert(flip_optimizer_guard_status(&status) == 0);
    assert(status.active == 0);
    assert(status.stock_only == 0);
    assert(strcmp(status.last_action, "confirmed") == 0);
    assert(strcmp(status.last_trial_id, "trial-001") == 0);
    assert(status.last_epoch == 111);
}

static void test_cancel_matching_trial(void)
{
    reset_root();

    assert(flip_optimizer_guard_arm("table-only", 150) == 0);
    assert(flip_optimizer_guard_cancel("wrong", 151) < 0);

    flip_optimizer_guard_status_t status;
    assert(flip_optimizer_guard_status(&status) == 0);
    assert(status.active == 1);
    assert(status.stock_only == 0);

    assert(flip_optimizer_guard_cancel("table-only", 152) == 0);

    assert(flip_optimizer_guard_status(&status) == 0);
    assert(status.active == 0);
    assert(status.stock_only == 0);
    assert(strcmp(status.last_action, "cancelled") == 0);
    assert(strcmp(status.last_trial_id, "table-only") == 0);
    assert(status.last_epoch == 152);
}


static void test_fail_matching_trial(void)
{
    reset_root();

    assert(flip_optimizer_guard_arm("live-fail", 170) == 0);
    assert(flip_optimizer_guard_fail("wrong", 171) < 0);
    assert(flip_optimizer_guard_fail("live-fail", 172) == 0);

    flip_optimizer_guard_status_t status;
    assert(flip_optimizer_guard_status(&status) == 0);
    assert(status.active == 0);
    assert(status.stock_only == 0);
    assert(strcmp(status.last_action, "failed") == 0);
    assert(strcmp(status.last_trial_id, "live-fail") == 0);
    assert(status.last_epoch == 172);
}

static void test_corrupt_active_recovers_armed_trial_id(void)
{
    reset_root();

    assert(flip_optimizer_guard_arm(
               "journal-recovery-trial",
               600) == 0);

    FILE *file = fopen(
        FLIP_OPTIMIZER_GUARD_ACTIVE_FILE,
        "w");
    assert(file != NULL);
    assert(fclose(file) == 0);

    flip_optimizer_guard_boot_inspection_t inspection;
    assert(flip_optimizer_guard_boot_inspect(
               &inspection) == 0);
    assert(inspection.state ==
           FLIP_OPTIMIZER_GUARD_BOOT_CORRUPT);
    assert(strcmp(
               inspection.trial_id,
               "journal-recovery-trial") == 0);
    assert(inspection.armed_epoch == 600);

    char rolled[FLIP_OPTIMIZER_GUARD_TRIAL_ID_MAX + 1];
    assert(flip_optimizer_guard_boot_check(
               601,
               rolled,
               sizeof(rolled)) == 1);
    assert(strcmp(
               rolled,
               "journal-recovery-trial") == 0);

    flip_optimizer_guard_status_t status;
    assert(flip_optimizer_guard_status(&status) == 0);
    assert(status.active == 0);
    assert(status.stock_only == 1);
    assert(strcmp(
               status.last_action,
               "rollback_corrupt") == 0);
    assert(strcmp(
               status.last_trial_id,
               "journal-recovery-trial") == 0);
}

static void test_pmu_token_recovers_when_fat_records_corrupt(void)
{
    reset_root();

    const char *trial = "cpu-1800000-1037500-1";

    assert(flip_optimizer_guard_arm(
               trial,
               700) == 0);

    char pmu_trial[FLIP_OPTIMIZER_GUARD_TRIAL_ID_MAX + 1];
    flip_optimizer_pmu_phase_t phase;
    assert(flip_optimizer_pmu_token_read_trial_phase(
               pmu_trial,
               sizeof(pmu_trial),
               &phase) == 1);
    assert(strcmp(pmu_trial, trial) == 0);
    assert(phase == FLIP_OPTIMIZER_PMU_PHASE_PREPARED);

    assert(flip_optimizer_guard_mark_entering(trial) == 0);
    assert(flip_optimizer_pmu_token_read_trial_phase(
               pmu_trial,
               sizeof(pmu_trial),
               &phase) == 1);
    assert(phase == FLIP_OPTIMIZER_PMU_PHASE_ENTERING);

    FILE *active = fopen(
        FLIP_OPTIMIZER_GUARD_ACTIVE_FILE,
        "w");
    assert(active != NULL);
    assert(fclose(active) == 0);

    FILE *event = fopen(
        FLIP_OPTIMIZER_GUARD_EVENT_FILE,
        "w");
    assert(event != NULL);
    assert(fclose(event) == 0);

    flip_optimizer_guard_boot_inspection_t inspection;
    assert(flip_optimizer_guard_boot_inspect(
               &inspection) == 0);
    assert(inspection.state ==
           FLIP_OPTIMIZER_GUARD_BOOT_CORRUPT);
    assert(strcmp(inspection.trial_id, trial) == 0);

    char rolled[FLIP_OPTIMIZER_GUARD_TRIAL_ID_MAX + 1];
    assert(flip_optimizer_guard_boot_check(
               701,
               rolled,
               sizeof(rolled)) == 1);
    assert(strcmp(rolled, trial) == 0);

    flip_optimizer_guard_status_t status;
    assert(flip_optimizer_guard_status(&status) == 0);
    assert(status.active == 0);
    assert(status.stock_only == 1);
    assert(strcmp(status.last_action, "rollback_corrupt") == 0);
    assert(strcmp(status.last_trial_id, trial) == 0);

    assert(flip_optimizer_pmu_token_read_trial(
               pmu_trial,
               sizeof(pmu_trial)) == 0);
}

static void test_prepared_cpu_trial_is_cancelled_on_boot(void)
{
    reset_root();

    const char *trial = "cpu-1416000-912500-1";
    assert(flip_optimizer_guard_arm(
               trial,
               720) == 0);

    char pmu_trial[FLIP_OPTIMIZER_GUARD_TRIAL_ID_MAX + 1];
    flip_optimizer_pmu_phase_t phase;
    assert(flip_optimizer_pmu_token_read_trial_phase(
               pmu_trial,
               sizeof(pmu_trial),
               &phase) == 1);
    assert(strcmp(pmu_trial, trial) == 0);
    assert(phase == FLIP_OPTIMIZER_PMU_PHASE_PREPARED);

    flip_optimizer_guard_boot_inspection_t inspection;
    assert(flip_optimizer_guard_boot_inspect(
               &inspection) == 0);
    assert(inspection.state ==
           FLIP_OPTIMIZER_GUARD_BOOT_PREPARED);
    assert(strcmp(inspection.trial_id, trial) == 0);

    char settled[FLIP_OPTIMIZER_GUARD_TRIAL_ID_MAX + 1];
    assert(flip_optimizer_guard_boot_check(
               721,
               settled,
               sizeof(settled)) == 0);
    assert(strcmp(settled, trial) == 0);

    flip_optimizer_guard_status_t status;
    assert(flip_optimizer_guard_status(&status) == 0);
    assert(status.active == 0);
    assert(status.stock_only == 0);
    assert(strcmp(
               status.last_action,
               "cancelled_unentered") == 0);
    assert(strcmp(status.last_trial_id, trial) == 0);

    assert(flip_optimizer_pmu_token_read_trial(
               pmu_trial,
               sizeof(pmu_trial)) == 0);
}

static void test_boot_rolls_back_unconfirmed_trial(void)
{
    reset_root();

    assert(flip_optimizer_guard_arm("trial-crash", 200) == 0);

    char rolled[FLIP_OPTIMIZER_GUARD_TRIAL_ID_MAX + 1];
    assert(flip_optimizer_guard_boot_check(
               210,
               rolled,
               sizeof(rolled)) == 1);
    assert(strcmp(rolled, "trial-crash") == 0);

    flip_optimizer_guard_status_t status;
    assert(flip_optimizer_guard_status(&status) == 0);
    assert(status.active == 0);
    assert(status.stock_only == 1);
    assert(strcmp(status.last_action, "rolled_back") == 0);
    assert(strcmp(status.last_trial_id, "trial-crash") == 0);
    assert(status.last_epoch == 210);

    assert(flip_optimizer_guard_arm("retry", 220) < 0);
    assert(flip_optimizer_guard_clear_stock_only() == 0);
    assert(flip_optimizer_guard_arm("retry", 221) == 0);
}

static void test_corrupt_trial_fails_safe(void)
{
    reset_root();

    assert(system(
        "mkdir -p " FLIP_OPTIMIZER_GUARD_ROOT " && "
        "printf 'garbage=1\\n' > "
        FLIP_OPTIMIZER_GUARD_ACTIVE_FILE) == 0);

    char rolled[FLIP_OPTIMIZER_GUARD_TRIAL_ID_MAX + 1];
    assert(flip_optimizer_guard_boot_check(
               300,
               rolled,
               sizeof(rolled)) == 1);
    assert(strcmp(rolled, "unknown") == 0);

    flip_optimizer_guard_status_t status;
    assert(flip_optimizer_guard_status(&status) == 0);
    assert(status.active == 0);
    assert(status.stock_only == 1);
    assert(strcmp(status.last_action, "rollback_corrupt") == 0);
    assert(access(FLIP_OPTIMIZER_GUARD_ACTIVE_FILE, F_OK) != 0);
}

static void test_trial_id_validation(void)
{
    reset_root();

    assert(flip_optimizer_guard_arm("", 1) < 0);
    assert(flip_optimizer_guard_arm("../bad", 1) < 0);
    assert(flip_optimizer_guard_arm("bad trial", 1) < 0);
    assert(flip_optimizer_guard_arm("safe_trial.1", 1) == 0);
}

int main(void)
{
    assert(flip_optimizer_guard_status(NULL) < 0);

    test_arm_and_confirm();
    test_cancel_matching_trial();
    test_fail_matching_trial();
    test_corrupt_active_recovers_armed_trial_id();
    test_pmu_token_recovers_when_fat_records_corrupt();
    test_prepared_cpu_trial_is_cancelled_on_boot();
    test_boot_rolls_back_unconfirmed_trial();
    test_corrupt_trial_fails_safe();
    test_trial_id_validation();

    printf("flip_optimizer_guard tests passed\n");
    return EXIT_SUCCESS;
}
