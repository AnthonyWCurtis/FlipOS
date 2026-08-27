#ifndef FLIP_STATE_H
#define FLIP_STATE_H

#include "flip_core.h"

#ifndef FLIP_STATE_ROOT
#define FLIP_STATE_ROOT "/mnt/SDCARD/flipos-dev/states"
#endif

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

int flip_state_preserve_recovery(
    const flip_state_context_t *context);

int flip_state_quarantine_current(
    const flip_state_context_t *context);

int flip_state_restore_recovery(
    const flip_state_context_t *context);

#endif
