#ifndef FLIP_RECOVERY_H
#define FLIP_RECOVERY_H

#ifndef FLIP_SESSION_RECOVERY_ROOT
#define FLIP_SESSION_RECOVERY_ROOT "/mnt/SDCARD/flipos-dev/sessions"
#endif

typedef struct {
    const char *system_id;
    const char *rom_path;
} flip_recovery_context_t;

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

#endif
