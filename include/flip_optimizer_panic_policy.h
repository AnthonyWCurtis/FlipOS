#ifndef FLIP_OPTIMIZER_PANIC_POLICY_H
#define FLIP_OPTIMIZER_PANIC_POLICY_H

#ifndef FLIP_OPTIMIZER_SYSCTL_ROOT
#define FLIP_OPTIMIZER_SYSCTL_ROOT "/proc/sys/kernel"
#endif

typedef struct {
    int panic_seconds;
    int panic_on_oops;
    int panic_on_warn;
    int panic_on_rcu_stall;
    int active;
} flip_optimizer_panic_policy_t;

int flip_optimizer_panic_policy_begin(
    flip_optimizer_panic_policy_t *policy);

int flip_optimizer_panic_policy_end(
    flip_optimizer_panic_policy_t *policy);

#endif
