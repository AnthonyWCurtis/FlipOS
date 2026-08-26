#ifndef FLIP_SYSTEM_H
#define FLIP_SYSTEM_H

#include "flip_session_adapter.h"

typedef struct flip_system {
    const char *id;
    const char *display_name;

    const char *runner;
    const char *rom_directory;

    const char *const *rom_extensions;
    int rom_extension_count;

    const flip_session_adapter_t *session_adapter;
} flip_system_t;

const flip_system_t *
flip_system_find(
    const char *id);

/*
 * Registry enumeration.
 *
 * These APIs let the frontend discover systems without
 * hard-coding individual system IDs.
 */
int flip_system_count(void);

const flip_system_t *
flip_system_at(
    int index);

const flip_system_t *
flip_system_default(void);

/*
 * Return non-zero when this system has a persistent
 * automatic resume state for the specified game.
 */
int flip_system_resume_available(
    const flip_system_t *system,
    const char *rom_path);

int flip_system_supports_path(
    const flip_system_t *system,
    const char *path);

#endif
