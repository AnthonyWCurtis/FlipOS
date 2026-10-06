#include "flip_optimizer_guard.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *program)
{
    fprintf(
        stderr,
        "usage: %s status | arm TRIAL_ID | confirm TRIAL_ID | cancel TRIAL_ID | "
        "boot-check | clear-stock-only\n",
        program);
}

static int print_status(void)
{
    flip_optimizer_guard_status_t status;

    if (flip_optimizer_guard_status(&status) < 0)
        return -1;

    printf("active=%d\n", status.active);
    printf("stock_only=%d\n", status.stock_only);

    if (status.active) {
        printf("trial_id=%s\n", status.trial_id);
        printf("armed_epoch=%u\n", status.armed_epoch);
    }

    if (status.last_action[0]) {
        printf("last_action=%s\n", status.last_action);
        printf("last_trial_id=%s\n", status.last_trial_id);
        printf("last_epoch=%u\n", status.last_epoch);
    }

    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "status") == 0)
        return print_status() == 0 ? EXIT_SUCCESS : EXIT_FAILURE;

    if (argc == 3 && strcmp(argv[1], "arm") == 0) {
        if (flip_optimizer_guard_arm(argv[2], 0) < 0)
            return EXIT_FAILURE;

        printf("OPTIMIZER_GUARD: armed trial=%s\n", argv[2]);
        return EXIT_SUCCESS;
    }

    if (argc == 3 && strcmp(argv[1], "confirm") == 0) {
        if (flip_optimizer_guard_confirm(argv[2], 0) < 0)
            return EXIT_FAILURE;

        printf("OPTIMIZER_GUARD: confirmed trial=%s\n", argv[2]);
        return EXIT_SUCCESS;
    }

    if (argc == 3 && strcmp(argv[1], "cancel") == 0) {
        if (flip_optimizer_guard_cancel(argv[2], 0) < 0)
            return EXIT_FAILURE;

        printf("OPTIMIZER_GUARD: cancelled trial=%s\n", argv[2]);
        return EXIT_SUCCESS;
    }

    if (argc == 2 && strcmp(argv[1], "boot-check") == 0) {
        char trial_id[FLIP_OPTIMIZER_GUARD_TRIAL_ID_MAX + 1];
        int rc = flip_optimizer_guard_boot_check(
            0,
            trial_id,
            sizeof(trial_id));

        if (rc < 0)
            return EXIT_FAILURE;

        if (rc == 0) {
            flip_optimizer_guard_status_t status;

            if (flip_optimizer_guard_status(&status) < 0)
                return EXIT_FAILURE;

            printf(
                "OPTIMIZER_GUARD: boot safe stock_only=%d\n",
                status.stock_only);
        } else
            printf(
                "OPTIMIZER_GUARD: rollback trial=%s stock_only=1\n",
                trial_id);

        return EXIT_SUCCESS;
    }

    if (argc == 2 && strcmp(argv[1], "clear-stock-only") == 0) {
        if (flip_optimizer_guard_clear_stock_only() < 0)
            return EXIT_FAILURE;

        printf("OPTIMIZER_GUARD: stock-only latch cleared\n");
        return EXIT_SUCCESS;
    }

    usage(argv[0]);
    return EXIT_FAILURE;
}
