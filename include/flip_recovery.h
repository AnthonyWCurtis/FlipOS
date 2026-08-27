#ifndef FLIP_RECOVERY_H
#define FLIP_RECOVERY_H

#include "flip_core.h"
#include "flip_save.h"
#include "flip_state.h"

#ifndef FLIP_SESSION_RECOVERY_ROOT
#define FLIP_SESSION_RECOVERY_ROOT "/mnt/SDCARD/flipos-dev/sessions"
#endif

typedef struct {
    const char *system_id;
    const char *rom_path;
} flip_recovery_context_t;

typedef enum {
    FLIP_RECOVERY_OK = 0,
    FLIP_RECOVERY_NO_PREVIOUS = 1,
    FLIP_RECOVERY_NO_UNDO = 2,
    FLIP_RECOVERY_ERROR = 3
} flip_recovery_result_t;

void flip_recovery_context_init(
    flip_recovery_context_t *context,
    const char *system_id,
    const char *rom_path);

int flip_recovery_resume_blocked(
    const flip_recovery_context_t *context);

int flip_recovery_block_resume(
    const flip_recovery_context_t *context);

int flip_recovery_clear_resume_block(
    const flip_recovery_context_t *context);

flip_recovery_result_t flip_recovery_restore_previous(
    flip_core_adapter_t *core,
    const flip_save_context_t *save_context,
    const flip_state_context_t *state_context,
    const flip_recovery_context_t *recovery_context);

flip_recovery_result_t flip_recovery_undo(
    flip_core_adapter_t *core,
    const flip_save_context_t *save_context,
    const flip_state_context_t *state_context,
    const flip_recovery_context_t *recovery_context);

#endif
