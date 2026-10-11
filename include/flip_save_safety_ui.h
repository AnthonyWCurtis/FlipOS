#ifndef FLIP_SAVE_SAFETY_UI_H
#define FLIP_SAVE_SAFETY_UI_H

#include "flip_system.h"

typedef enum {
    FLIP_SAVE_SAFETY_HEALTH_UNKNOWN = 0,
    FLIP_SAVE_SAFETY_HEALTH_HEALTHY,
    FLIP_SAVE_SAFETY_HEALTH_MISSING,
    FLIP_SAVE_SAFETY_HEALTH_INVALID
} flip_save_safety_health_t;

typedef struct {
    flip_save_safety_health_t current;
    flip_save_safety_health_t previous;
    flip_save_safety_health_t recovery;
    int resume_blocked;
    flip_save_safety_health_t undo_state;
} flip_save_safety_status_t;

int flip_save_safety_supported(const flip_system_t *system);
int flip_save_safety_user_enabled(void);

int flip_save_safety_build_start_normal(
    const flip_system_t *system,
    const char *rom_path,
    char **argv,
    size_t argv_count);

int flip_save_safety_query(
    const flip_system_t *system,
    const char *rom_path,
    flip_save_safety_status_t *status);

int flip_save_safety_restore_previous(
    const flip_system_t *system,
    const char *rom_path);

int flip_save_safety_undo(
    const flip_system_t *system,
    const char *rom_path);

const char *flip_save_safety_health_name(
    flip_save_safety_health_t health);

#endif
