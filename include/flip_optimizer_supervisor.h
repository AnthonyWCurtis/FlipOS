#ifndef FLIP_OPTIMIZER_SUPERVISOR_H
#define FLIP_OPTIMIZER_SUPERVISOR_H

#include "flip_optimizer_guard.h"

#define FLIP_OPTIMIZER_SUPERVISOR_RAM_LATCH \
    "/tmp/flipos-optimizer-stock-safe"

#ifndef FLIP_OPTIMIZER_BOOT_MODE_PATH
#define FLIP_OPTIMIZER_BOOT_MODE_PATH "/sys/kernel/boot_mode"
#endif

#define FLIP_OPTIMIZER_BOOT_MODE_MAX 32

typedef enum {
    FLIP_OPTIMIZER_RESET_UNKNOWN = 0,
    FLIP_OPTIMIZER_RESET_COLDBOOT = 1,
    FLIP_OPTIMIZER_RESET_NORMAL = 2,
    FLIP_OPTIMIZER_RESET_PANIC = 3,
    FLIP_OPTIMIZER_RESET_WATCHDOG = 4,
    FLIP_OPTIMIZER_RESET_OTHER = 5
} flip_optimizer_reset_cause_t;

typedef enum {
    FLIP_OPTIMIZER_SUPERVISOR_REASON_CLEAN = 0,
    FLIP_OPTIMIZER_SUPERVISOR_REASON_UNCONFIRMED = 1,
    FLIP_OPTIMIZER_SUPERVISOR_REASON_CORRUPT = 2,
    FLIP_OPTIMIZER_SUPERVISOR_REASON_UNREADABLE = 3,
    FLIP_OPTIMIZER_SUPERVISOR_REASON_STOCK_ONLY = 4,
    FLIP_OPTIMIZER_SUPERVISOR_REASON_PREPARED = 5
} flip_optimizer_supervisor_reason_t;

typedef struct {
    int stock_safe_required;
    int durable_settlement_required;
    flip_optimizer_supervisor_reason_t reason;
    flip_optimizer_reset_cause_t reset_cause;
    char boot_mode[FLIP_OPTIMIZER_BOOT_MODE_MAX];
    char trial_id[FLIP_OPTIMIZER_GUARD_TRIAL_ID_MAX + 1];
} flip_optimizer_supervisor_boot_decision_t;

int flip_optimizer_supervisor_boot_decide(
    flip_optimizer_supervisor_boot_decision_t *decision);

int flip_optimizer_supervisor_boot_enforce(
    flip_optimizer_supervisor_boot_decision_t *decision);

int flip_optimizer_supervisor_runtime_stock_safe(void);

const char *flip_optimizer_supervisor_reason_name(
    flip_optimizer_supervisor_reason_t reason);

const char *flip_optimizer_supervisor_reset_name(
    flip_optimizer_reset_cause_t cause);

#endif
