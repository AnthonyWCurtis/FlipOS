#ifndef FLIP_OPTIMIZER_SUPERVISOR_H
#define FLIP_OPTIMIZER_SUPERVISOR_H

#include "flip_optimizer_guard.h"

#define FLIP_OPTIMIZER_SUPERVISOR_RAM_LATCH \
    "/tmp/flipos-optimizer-stock-safe"

typedef enum {
    FLIP_OPTIMIZER_SUPERVISOR_REASON_CLEAN = 0,
    FLIP_OPTIMIZER_SUPERVISOR_REASON_UNCONFIRMED = 1,
    FLIP_OPTIMIZER_SUPERVISOR_REASON_CORRUPT = 2,
    FLIP_OPTIMIZER_SUPERVISOR_REASON_UNREADABLE = 3
} flip_optimizer_supervisor_reason_t;

typedef struct {
    int stock_safe_required;
    int durable_settlement_required;
    flip_optimizer_supervisor_reason_t reason;
    char trial_id[FLIP_OPTIMIZER_GUARD_TRIAL_ID_MAX + 1];
} flip_optimizer_supervisor_boot_decision_t;

int flip_optimizer_supervisor_boot_decide(
    flip_optimizer_supervisor_boot_decision_t *decision);

int flip_optimizer_supervisor_boot_enforce(
    flip_optimizer_supervisor_boot_decision_t *decision);

int flip_optimizer_supervisor_runtime_stock_safe(void);

const char *flip_optimizer_supervisor_reason_name(
    flip_optimizer_supervisor_reason_t reason);

#endif
