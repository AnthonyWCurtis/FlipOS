#ifndef FLIP_SESSION_PERSISTENCE_H
#define FLIP_SESSION_PERSISTENCE_H

#include <stddef.h>

typedef struct {
    int valid;

    char state[32];
    char system[32];
    char rom[512];
    char program[512];

    int pid;

    long long started;
    long long ended;

    int exit_code;
    int signal_number;
} flip_session_state_t;

/*
 * Read the version-1 current-session record.
 *
 * Returns:
 *   1 = valid record loaded
 *   0 = no current-session record
 *  -1 = malformed record or I/O error
 */
int flip_session_persistence_read_current(
    flip_session_state_t *state);

/*
 * Atomically commit the version-1 current-session record.
 *
 * The staged file is flushed and fsynced before rename.
 */
int flip_session_persistence_write_current(
    const flip_session_state_t *state);

/*
 * Move the current record aside as current.invalid.<epoch>.
 *
 * Returns:
 *   1 = record quarantined
 *   0 = no record existed
 *  -1 = error
 */
int flip_session_persistence_quarantine_current(
    char *quarantined_path,
    size_t path_size);

#endif
