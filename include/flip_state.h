#ifndef FLIP_STATE_H
#define FLIP_STATE_H

#include "flip_core.h"
#include "flip_paths.h"

typedef struct {
    const char *system_id;
    const char *rom_path;
    int current_was_restored;
} flip_state_context_t;

typedef enum {
    FLIP_STATE_RESTORE_NONE = 0,
    FLIP_STATE_RESTORE_CURRENT = 1,
    FLIP_STATE_RESTORE_PREVIOUS = 2,
    FLIP_STATE_RESTORE_UNUSABLE = 3,
    FLIP_STATE_RESTORE_ERROR = 4
} flip_state_restore_result_t;

typedef enum {
    FLIP_STATE_HEALTH_MISSING = 0,
    FLIP_STATE_HEALTH_AVAILABLE = 1,
    FLIP_STATE_HEALTH_INVALID = 2,
    FLIP_STATE_HEALTH_ERROR = 3
} flip_state_health_t;

typedef flip_state_health_t flip_state_recovery_health_t;

#define FLIP_STATE_RECOVERY_MISSING FLIP_STATE_HEALTH_MISSING
#define FLIP_STATE_RECOVERY_AVAILABLE FLIP_STATE_HEALTH_AVAILABLE
#define FLIP_STATE_RECOVERY_INVALID FLIP_STATE_HEALTH_INVALID
#define FLIP_STATE_RECOVERY_ERROR FLIP_STATE_HEALTH_ERROR

typedef struct {
    flip_state_health_t current;
    flip_state_health_t previous;
    flip_state_health_t recovery;
} flip_state_status_t;

void flip_state_context_init(
    flip_state_context_t *context,
    const char *system_id,
    const char *rom_path);

int flip_state_save(
    flip_core_adapter_t *core,
    flip_state_context_t *context);

flip_state_restore_result_t flip_state_restore(
    flip_core_adapter_t *core,
    flip_state_context_t *context);

int flip_state_query_status(
    flip_core_adapter_t *core,
    const flip_state_context_t *context,
    flip_state_status_t *status);

flip_state_recovery_health_t flip_state_recovery_health(
    flip_core_adapter_t *core,
    const flip_state_context_t *context);

int flip_state_preserve_recovery(
    const flip_state_context_t *context);

int flip_state_quarantine_current(
    const flip_state_context_t *context);

int flip_state_restore_recovery(
    const flip_state_context_t *context);

#endif
