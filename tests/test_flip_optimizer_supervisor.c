#include "flip_optimizer_supervisor.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void reset_root(void)
{
    assert(system("chmod -R u+rwX " FLIP_OPTIMIZER_GUARD_ROOT " 2>/dev/null || true") == 0);
    assert(system("rm -rf " FLIP_OPTIMIZER_GUARD_ROOT) == 0);
}

static void test_clean_boot(void)
{
    reset_root();

    flip_optimizer_supervisor_boot_decision_t decision;
    assert(flip_optimizer_supervisor_boot_decide(&decision) == 0);
    assert(decision.stock_safe_required == 0);
    assert(decision.durable_settlement_required == 0);
    assert(decision.reason == FLIP_OPTIMIZER_SUPERVISOR_REASON_CLEAN);
    assert(decision.trial_id[0] == '\0');
}

static void test_unconfirmed_trial_forces_stock(void)
{
    reset_root();
    assert(flip_optimizer_guard_arm("cpu-1992000-850000-20", 100) == 0);

    flip_optimizer_supervisor_boot_decision_t decision;
    assert(flip_optimizer_supervisor_boot_decide(&decision) == 0);
    assert(decision.stock_safe_required == 1);
    assert(decision.durable_settlement_required == 1);
    assert(decision.reason ==
           FLIP_OPTIMIZER_SUPERVISOR_REASON_UNCONFIRMED);
    assert(strcmp(decision.trial_id, "cpu-1992000-850000-20") == 0);
}

static void test_corrupt_trial_forces_stock(void)
{
    reset_root();
    assert(system(
        "mkdir -p " FLIP_OPTIMIZER_GUARD_ROOT " && "
        "printf 'garbage=1\\n' > "
        FLIP_OPTIMIZER_GUARD_ACTIVE_FILE) == 0);

    flip_optimizer_supervisor_boot_decision_t decision;
    assert(flip_optimizer_supervisor_boot_decide(&decision) == 0);
    assert(decision.stock_safe_required == 1);
    assert(decision.durable_settlement_required == 1);
    assert(decision.reason == FLIP_OPTIMIZER_SUPERVISOR_REASON_CORRUPT);
}

static void test_read_only_state_still_makes_safe_decision(void)
{
    reset_root();
    assert(flip_optimizer_guard_arm("read-only-trial", 200) == 0);
    assert(chmod(FLIP_OPTIMIZER_GUARD_ROOT, 0555) == 0);

    flip_optimizer_supervisor_boot_decision_t decision;
    assert(flip_optimizer_supervisor_boot_decide(&decision) == 0);
    assert(decision.stock_safe_required == 1);
    assert(decision.durable_settlement_required == 1);
    assert(decision.reason ==
           FLIP_OPTIMIZER_SUPERVISOR_REASON_UNCONFIRMED);
    assert(strcmp(decision.trial_id, "read-only-trial") == 0);

    assert(chmod(FLIP_OPTIMIZER_GUARD_ROOT, 0755) == 0);
}

static void test_unreadable_active_path_fails_safe(void)
{
    reset_root();
    assert(system(
        "mkdir -p " FLIP_OPTIMIZER_GUARD_ACTIVE_FILE) == 0);

    flip_optimizer_supervisor_boot_decision_t decision;
    assert(flip_optimizer_supervisor_boot_decide(&decision) == 0);
    assert(decision.stock_safe_required == 1);
    assert(decision.durable_settlement_required == 1);
    assert(decision.reason !=
           FLIP_OPTIMIZER_SUPERVISOR_REASON_CLEAN);
}


static void test_boot_enforce_uses_ram_latch(void)
{
    reset_root();
    unlink(FLIP_OPTIMIZER_SUPERVISOR_RAM_LATCH);

    flip_optimizer_supervisor_boot_decision_t decision;
    assert(flip_optimizer_supervisor_boot_enforce(&decision) == 0);
    assert(decision.stock_safe_required == 0);
    assert(flip_optimizer_supervisor_runtime_stock_safe() == 0);

    assert(flip_optimizer_guard_arm("runtime-latch-trial", 300) == 0);
    assert(chmod(FLIP_OPTIMIZER_GUARD_ROOT, 0555) == 0);

    assert(flip_optimizer_supervisor_boot_enforce(&decision) == 0);
    assert(decision.stock_safe_required == 1);
    assert(flip_optimizer_supervisor_runtime_stock_safe() == 1);

    assert(chmod(FLIP_OPTIMIZER_GUARD_ROOT, 0755) == 0);
    unlink(FLIP_OPTIMIZER_SUPERVISOR_RAM_LATCH);
}

int main(void)
{
    test_clean_boot();
    test_unconfirmed_trial_forces_stock();
    test_corrupt_trial_forces_stock();
    test_read_only_state_still_makes_safe_decision();
    test_unreadable_active_path_fails_safe();
    test_boot_enforce_uses_ram_latch();
    reset_root();
    unlink(FLIP_OPTIMIZER_SUPERVISOR_RAM_LATCH);

    printf("flip_optimizer_supervisor tests passed\n");
    return EXIT_SUCCESS;
}
