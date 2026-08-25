#ifndef FLIP_LIBRARY_H
#define FLIP_LIBRARY_H

#include "flip_system.h"

#include <stdint.h>
#include <stddef.h>

/*
 * Compute a cheap content fingerprint for one system's
 * ROM directory.
 *
 * The fingerprint is based on supported directory entries,
 * filenames, file sizes and mtimes. ROM contents are not read.
 *
 * Returns:
 *   0  = success
 *  -1  = directory/error
 */
int flip_library_fingerprint(
    const flip_system_t *system,
    uint64_t *fingerprint,
    int *rom_count);

/*
 * Read the last persisted fingerprint for a system.
 *
 * Returns:
 *   1 = cache loaded
 *   0 = no cache exists
 *  -1 = invalid/error
 */
int flip_library_load_fingerprint(
    const flip_system_t *system,
    uint64_t *fingerprint,
    int *rom_count);

/*
 * Atomically persist the current fingerprint.
 *
 * Returns:
 *   0 = success
 *  -1 = error
 */
int flip_library_save_fingerprint(
    const flip_system_t *system,
    uint64_t fingerprint,
    int rom_count);

/*
 * Atomically persist the ordered ROM filename index for
 * one system.
 *
 * The caller owns the filename storage. Each entry must be
 * a plain filename, not a full path.
 *
 * Returns:
 *   0 = success
 *  -1 = error
 */
int flip_library_save_index(
    const flip_system_t *system,
    const char *const *names,
    int count);

/*
 * Load the persisted ROM filename index.
 *
 * names points to caller-owned fixed-size storage:
 *
 *     char names[max_names][name_size];
 *
 * Returns:
 *  >=0 = number of entries loaded
 *   -1 = missing, invalid or unreadable index
 */
int flip_library_load_index(
    const flip_system_t *system,
    char *names,
    int max_names,
    size_t name_size);

#endif
