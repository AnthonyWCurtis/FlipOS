#ifndef FLIP_OPTIMIZER_GUARD_H
#define FLIP_OPTIMIZER_GUARD_H

#include <stddef.h>

#define FLIP_OPTIMIZER_GUARD_VERSION 1
#define FLIP_OPTIMIZER_GUARD_TRIAL_ID_MAX 64
#define FLIP_OPTIMIZER_GUARD_ACTION_MAX 32

#ifndef FLIP_OPTIMIZER_GUARD_ROOT
#define FLIP_OPTIMIZER_GUARD_ROOT "/mnt/SDCARD/flipos-dev/state/optimizer"
#endif

#define FLIP_OPTIMIZER_GUARD_ACTIVE_FILE \
    FLIP_OPTIMIZER_GUARD_ROOT "/trial-active"
#define FLIP_OPTIMIZER_GUARD_EVENT_FILE \
    FLIP_OPTIMIZER_GUARD_ROOT "/trial-last-event"
#define FLIP_OPTIMIZER_GUARD_STOCK_ONLY_FILE \
    FLIP_OPTIMIZER_GUARD_ROOT "/stock-only"

typedef struct {
    int active;
    int stock_only;
    char trial_id[FLIP_OPTIMIZER_GUARD_TRIAL_ID_MAX + 1];
    unsigned armed_epoch;
    char last_action[FLIP_OPTIMIZER_GUARD_ACTION_MAX];
    char last_trial_id[FLIP_OPTIMIZER_GUARD_TRIAL_ID_MAX + 1];
    unsigned last_epoch;
} flip_optimizer_guard_status_t;

int flip_optimizer_guard_arm(const char *trial_id, unsigned epoch);
int flip_optimizer_guard_confirm(const char *trial_id, unsigned epoch);
int flip_optimizer_guard_boot_check(
    unsigned epoch,
    char *rolled_back_trial,
    size_t rolled_back_trial_size);
int flip_optimizer_guard_clear_stock_only(void);
int flip_optimizer_guard_status(flip_optimizer_guard_status_t *status);

#endif
