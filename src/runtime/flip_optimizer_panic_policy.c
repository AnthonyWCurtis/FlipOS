#include "flip_optimizer_panic_policy.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#define PANIC_SECONDS_DURING_TEST 10
#define PANIC_ON_DURING_TEST 1

static int make_path(
    char *buffer,
    size_t size,
    const char *name)
{
    int n = snprintf(
        buffer,
        size,
        "%s/%s",
        FLIP_OPTIMIZER_SYSCTL_ROOT,
        name);

    return n >= 0 && (size_t)n < size ? 0 : -1;
}

static int read_int(
    const char *name,
    int *value)
{
    if (!name || !value)
        return -1;

    char path[256];
    if (make_path(path, sizeof(path), name) < 0)
        return -1;

    FILE *file = fopen(path, "r");
    if (!file)
        return -1;

    int parsed = 0;
    int rc = fscanf(file, "%d", &parsed);
    int close_rc = fclose(file);

    if (rc != 1 || close_rc != 0)
        return -1;

    *value = parsed;
    return 0;
}

static int write_int(
    const char *name,
    int value)
{
    if (!name)
        return -1;

    char path[256];
    if (make_path(path, sizeof(path), name) < 0)
        return -1;

    FILE *file = fopen(path, "w");
    if (!file)
        return -1;

    int failed =
        fprintf(file, "%d\n", value) < 0 ||
        fflush(file) != 0 ||
        fclose(file) != 0;

    return failed ? -1 : 0;
}

static int restore_values(
    flip_optimizer_panic_policy_t *policy)
{
    if (!policy)
        return -1;

    /*
     * Disable panic triggers before restoring the original timeout so there
     * is never a window with panic-on-* enabled and kernel.panic back at zero.
     */
    int failed = 0;

    if (write_int(
            "panic_on_rcu_stall",
            policy->panic_on_rcu_stall) < 0)
        failed = 1;
    if (write_int(
            "panic_on_warn",
            policy->panic_on_warn) < 0)
        failed = 1;
    if (write_int(
            "panic_on_oops",
            policy->panic_on_oops) < 0)
        failed = 1;
    if (write_int(
            "panic",
            policy->panic_seconds) < 0)
        failed = 1;

    policy->active = 0;
    return failed ? -1 : 0;
}

int flip_optimizer_panic_policy_begin(
    flip_optimizer_panic_policy_t *policy)
{
    if (!policy || policy->active)
        return -1;

    memset(policy, 0, sizeof(*policy));

    if (read_int(
            "panic",
            &policy->panic_seconds) < 0 ||
        read_int(
            "panic_on_oops",
            &policy->panic_on_oops) < 0 ||
        read_int(
            "panic_on_warn",
            &policy->panic_on_warn) < 0 ||
        read_int(
            "panic_on_rcu_stall",
            &policy->panic_on_rcu_stall) < 0)
        return -1;

    /*
     * Set the reboot timeout first. If a kernel fault happens while the
     * trigger flags are being enabled, it will still have a bounded restart.
     */
    if (write_int(
            "panic",
            PANIC_SECONDS_DURING_TEST) < 0 ||
        write_int(
            "panic_on_oops",
            PANIC_ON_DURING_TEST) < 0 ||
        write_int(
            "panic_on_warn",
            PANIC_ON_DURING_TEST) < 0 ||
        write_int(
            "panic_on_rcu_stall",
            PANIC_ON_DURING_TEST) < 0) {
        (void)restore_values(policy);
        return -1;
    }

    int verify_panic = 0;
    int verify_oops = 0;
    int verify_warn = 0;
    int verify_rcu = 0;

    if (read_int("panic", &verify_panic) < 0 ||
        read_int("panic_on_oops", &verify_oops) < 0 ||
        read_int("panic_on_warn", &verify_warn) < 0 ||
        read_int("panic_on_rcu_stall", &verify_rcu) < 0 ||
        verify_panic != PANIC_SECONDS_DURING_TEST ||
        verify_oops != PANIC_ON_DURING_TEST ||
        verify_warn != PANIC_ON_DURING_TEST ||
        verify_rcu != PANIC_ON_DURING_TEST) {
        (void)restore_values(policy);
        return -1;
    }

    policy->active = 1;
    return 0;
}

int flip_optimizer_panic_policy_end(
    flip_optimizer_panic_policy_t *policy)
{
    if (!policy)
        return -1;

    if (!policy->active)
        return 0;

    return restore_values(policy);
}
