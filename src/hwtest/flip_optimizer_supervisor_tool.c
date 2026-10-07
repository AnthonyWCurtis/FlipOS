#include "flip_optimizer_supervisor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *program)
{
    fprintf(stderr, "usage: %s boot-decision\n", program);
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "runtime-status") == 0) {
        printf(
            "runtime_stock_safe=%d\n",
            flip_optimizer_supervisor_runtime_stock_safe());
        return EXIT_SUCCESS;
    }

    flip_optimizer_supervisor_boot_decision_t decision;
    int rc = 0;

    if (strcmp(argv[1], "boot-decision") == 0)
        rc = flip_optimizer_supervisor_boot_decide(&decision);
    else if (strcmp(argv[1], "boot-enforce") == 0)
        rc = flip_optimizer_supervisor_boot_enforce(&decision);
    else {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (rc < 0)
        return EXIT_FAILURE;

    printf("stock_safe_required=%d\n", decision.stock_safe_required);
    printf(
        "durable_settlement_required=%d\n",
        decision.durable_settlement_required);
    printf(
        "reason=%s\n",
        flip_optimizer_supervisor_reason_name(decision.reason));
    printf(
        "trial_id=%s\n",
        decision.trial_id[0] ? decision.trial_id : "none");
    printf("boot_decision_writes=0\n");
    return EXIT_SUCCESS;
}
