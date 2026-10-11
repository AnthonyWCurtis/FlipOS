#ifndef FLIP_SESSION_H
#define FLIP_SESSION_H

#include "flip_drm.h"
#include "flip_session_persistence.h"

typedef struct flip_system flip_system_t;

typedef enum {
    FLIP_SESSION_ACTION_NONE = 0,
    FLIP_SESSION_ACTION_EXIT,
    FLIP_SESSION_ACTION_SUSPEND,
    FLIP_SESSION_ACTION_LOW_BATTERY,
    FLIP_SESSION_ACTION_ERROR
} flip_session_action_t;

typedef struct {
    const flip_system_t *system;
    const char *rom_path;

    /*
     * Non-zero only when the frontend is automatically continuing the
     * same session after a lid wake. Deliberate launches remain interactive.
     */
    int automatic_resume;
} flip_session_metadata_t;

typedef struct {
    int launched;
    int exited;
    int exit_code;

    int signaled;
    int signal_number;

    flip_session_action_t action;
} flip_session_result_t;

int flip_session_launch(
    flip_drm_t *drm,
    const char *program,
    char *const argv[],
    const flip_session_metadata_t *metadata,
    flip_session_result_t *result);

/*
 * Close the currently suspended FlipOS session.
 *
 * This only changes session lifecycle metadata.
 * Emulator savestates are deliberately preserved.
 */
/*
 * Read and validate the current persisted session metadata.
 *
 * Returns:
 *   1 = valid session metadata loaded
 *   0 = no current session file
 *  -1 = malformed/invalid session metadata
 */
int flip_session_read_current(
    flip_session_state_t *state);

/*
 * Repair a stale persisted "running" session.
 *
 * A running record whose process no longer exists, or whose
 * executable no longer matches the recorded emulator, is
 * rewritten as "interrupted".
 *
 * Emulator savestates and SRAM are never touched.
 *
 * Returns:
 *   1 = stale running session recovered
 *   0 = nothing needed
 *  -1 = recovery/read error
 */
int flip_session_recover_current(void);

int flip_session_close_suspended(void);

/*
 * Close one recoverable registry session identified by system and ROM.
 *
 * Suspended, interrupted, signaled and error sessions can be closed.
 * If the target is also the legacy current suspended session, update
 * current metadata through the existing close path. Otherwise remove
 * only the registry lifecycle entry. Emulator saves/states are preserved.
 *
 * Returns:
 *   1 = closed
 *   0 = target is not a suspended registry session
 *  -1 = error
 */
int flip_session_close_registered(
    const flip_system_t *system,
    const char *rom_path);

/*
 * Reconcile the persistent multi-session registry at frontend startup.
 *
 * - remove entries whose system/ROM is no longer launchable
 * - remove terminal/unknown lifecycle states
 * - convert orphaned "running" entries to "interrupted"
 * - preserve suspended/interrupted/error save and state data
 *
 * Returns the number of repaired/removed entries, 0 if clean, or -1.
 */
int flip_session_reconcile_registry(void);

/*
 * Remove one game from GameSwitcher/recent history.
 * Emulator savestates are not touched.
 */
int flip_session_remove_recent(
    const flip_system_t *system,
    const char *rom_path);

/*
 * Repair GameSwitcher history in place.
 *
 * Removes malformed, missing, unsupported and duplicate
 * entries. Emulator saves/states are never touched.
 */
int flip_session_prune_recent(void);

#endif
