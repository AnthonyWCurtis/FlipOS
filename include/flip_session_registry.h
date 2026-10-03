#ifndef FLIP_SESSION_REGISTRY_H
#define FLIP_SESSION_REGISTRY_H

#include <stddef.h>

#define FLIP_SESSION_REGISTRY_MAX 16

typedef struct {
    int valid;

    char system[32];
    char rom[512];
    char program[512];
    char state[32];

    long long started;
    long long updated;
} flip_session_registry_entry_t;

/*
 * Load the persistent GameSwitcher session registry.
 *
 * Entries are returned newest-first by updated timestamp.
 *
 * Returns:
 *   0 = success, including an empty/non-existent registry
 *  -1 = malformed registry or I/O error
 */
int flip_session_registry_load(
    flip_session_registry_entry_t *entries,
    size_t capacity,
    size_t *count);

/*
 * Insert or replace a session identified by (system, rom).
 * The entry becomes the newest registry entry.
 *
 * Persistence uses a staged file, file-data fsync(), then rename.
 */
int flip_session_registry_upsert(
    const flip_session_registry_entry_t *entry);

/*
 * Remove a session identified by (system, rom).
 *
 * Returns:
 *   1 = removed
 *   0 = not present
 *  -1 = error
 */
int flip_session_registry_remove(
    const char *system,
    const char *rom);

/*
 * Find one session identified by (system, rom).
 *
 * Returns:
 *   1 = found
 *   0 = not present
 *  -1 = error
 */
int flip_session_registry_find(
    const char *system,
    const char *rom,
    flip_session_registry_entry_t *entry);

#endif
