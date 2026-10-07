#include "flip_optimizer_panic_policy.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static int read_value(const char *name)
{
    char path[256];
    snprintf(
        path,
        sizeof(path),
        "%s/%s",
        FLIP_OPTIMIZER_SYSCTL_ROOT,
        name);

    FILE *file = fopen(path, "r");
    assert(file != NULL);

    int value = -1;
    assert(fscanf(file, "%d", &value) == 1);
    assert(fclose(file) == 0);
    return value;
}

static void write_value(const char *name, int value)
{
    char path[256];
    snprintf(
        path,
        sizeof(path),
        "%s/%s",
        FLIP_OPTIMIZER_SYSCTL_ROOT,
        name);

    FILE *file = fopen(path, "w");
    assert(file != NULL);
    assert(fprintf(file, "%d\n", value) > 0);
    assert(fclose(file) == 0);
}

int main(void)
{
    assert(system(
        "rm -rf " FLIP_OPTIMIZER_SYSCTL_ROOT " && "
        "mkdir -p " FLIP_OPTIMIZER_SYSCTL_ROOT) == 0);

    write_value("panic", 0);
    write_value("panic_on_oops", 0);
    write_value("panic_on_warn", 0);
    write_value("panic_on_rcu_stall", 0);

    flip_optimizer_panic_policy_t policy = {0};
    assert(flip_optimizer_panic_policy_begin(&policy) == 0);
    assert(policy.active == 1);
    assert(read_value("panic") == 10);
    assert(read_value("panic_on_oops") == 1);
    assert(read_value("panic_on_warn") == 1);
    assert(read_value("panic_on_rcu_stall") == 1);

    assert(flip_optimizer_panic_policy_end(&policy) == 0);
    assert(policy.active == 0);
    assert(read_value("panic") == 0);
    assert(read_value("panic_on_oops") == 0);
    assert(read_value("panic_on_warn") == 0);
    assert(read_value("panic_on_rcu_stall") == 0);

    assert(system(
        "rm -rf " FLIP_OPTIMIZER_SYSCTL_ROOT) == 0);

    printf("flip_optimizer_panic_policy tests passed\n");
    return 0;
}
