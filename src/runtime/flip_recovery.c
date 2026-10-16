#define _POSIX_C_SOURCE 200809L

#include "flip_recovery.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static uint64_t flip_recovery_path_hash(const char *text)
{
    uint64_t hash = UINT64_C(14695981039346656037);

    while (text && *text) {
        hash ^= (unsigned char)*text++;
        hash *= UINT64_C(1099511628211);
    }

    return hash;
}

void flip_recovery_context_init(
    flip_recovery_context_t *context,
    const char *system_id,
    const char *rom_path)
{
    if (!context)
        return;

    context->system_id = system_id;
    context->rom_path = rom_path;
}

int flip_recovery_test_build_path(
    const flip_recovery_context_t *context,
    const char *suffix,
    char *path,
    size_t path_size)
{
    if (!context ||
        !context->system_id ||
        !context->rom_path ||
        !suffix ||
        !path ||
        path_size == 0)
        return -1;

    uint64_t hash = flip_recovery_path_hash(context->rom_path);
    int written = snprintf(
        path,
        path_size,
        FLIP_SESSION_RECOVERY_ROOT "/%s/%016llx%s",
        context->system_id,
        (unsigned long long)hash,
        suffix);

    if (written < 0 || (size_t)written >= path_size)
        return -1;

    return 0;
}

static int ensure_directory(const char *path)
{
    if (mkdir(path, 0777) < 0 && errno != EEXIST)
        return -1;

    return 0;
}

static int ensure_recovery_directories(
    const flip_recovery_context_t *context)
{
    if (!context || !context->system_id)
        return -1;

    if (ensure_directory(FLIP_SESSION_RECOVERY_ROOT) < 0)
        return -1;

    char system_dir[512];
    int written = snprintf(
        system_dir,
        sizeof(system_dir),
        FLIP_SESSION_RECOVERY_ROOT "/%s",
        context->system_id);

    if (written < 0 || (size_t)written >= sizeof(system_dir))
        return -1;

    return ensure_directory(system_dir);
}

int flip_recovery_resume_blocked(
    const flip_recovery_context_t *context)
{
    char marker_path[512];

    if (flip_recovery_test_build_path(
            context,
            ".restore-pending",
            marker_path,
            sizeof(marker_path)) < 0)
        return 1;

    struct stat st;

    if (stat(marker_path, &st) < 0) {
        if (errno == ENOENT)
            return 0;

        fprintf(
            stderr,
            "FLIP_RECOVERY: MARKER_READ_ERROR path=%s error=%s\n",
            marker_path,
            strerror(errno));
        return 1;
    }

    if (!S_ISREG(st.st_mode) || st.st_size != 0) {
        fprintf(
            stderr,
            "FLIP_RECOVERY: MARKER_INVALID path=%s\n",
            marker_path);
        return 1;
    }

    return 1;
}

int flip_recovery_block_resume(
    const flip_recovery_context_t *context)
{
    if (ensure_recovery_directories(context) < 0)
        return -1;

    char tmp_path[512];
    char marker_path[512];

    if (flip_recovery_test_build_path(
            context,
            ".restore-pending.tmp",
            tmp_path,
            sizeof(tmp_path)) < 0 ||
        flip_recovery_test_build_path(
            context,
            ".restore-pending",
            marker_path,
            sizeof(marker_path)) < 0)
        return -1;

    FILE *file = fopen(tmp_path, "wb");
    if (!file)
        return -1;

    int ok = 1;

    if (fflush(file) != 0)
        ok = 0;

    if (ok && fsync(fileno(file)) < 0)
        ok = 0;

    if (fclose(file) != 0)
        ok = 0;

    if (!ok) {
        unlink(tmp_path);
        return -1;
    }

    struct stat st;
    if (stat(tmp_path, &st) < 0 ||
        !S_ISREG(st.st_mode) ||
        st.st_size != 0) {
        unlink(tmp_path);
        return -1;
    }

    if (rename(tmp_path, marker_path) < 0) {
        unlink(tmp_path);
        return -1;
    }

    fprintf(stderr, "FLIP_RECOVERY: RESUME_BLOCKED path=%s\n", marker_path);
    return 0;
}

int flip_recovery_clear_resume_block(
    const flip_recovery_context_t *context)
{
    char marker_path[512];

    if (flip_recovery_test_build_path(
            context,
            ".restore-pending",
            marker_path,
            sizeof(marker_path)) < 0)
        return -1;

    struct stat st;

    if (lstat(marker_path, &st) < 0) {
        if (errno == ENOENT)
            return 0;
        return -1;
    }

    if (!S_ISREG(st.st_mode))
        return -1;

    if (unlink(marker_path) < 0)
        return -1;

    fprintf(stderr, "FLIP_RECOVERY: RESUME_REENABLED path=%s\n", marker_path);
    return 0;
}

flip_recovery_result_t flip_recovery_restore_previous(
    flip_core_adapter_t *core,
    const flip_save_context_t *save_context,
    const flip_state_context_t *state_context,
    const flip_recovery_context_t *recovery_context)
{
    if (!core || !save_context || !state_context || !recovery_context)
        return FLIP_RECOVERY_ERROR;

    if (flip_save_health(core, save_context, ".sav.prev") !=
        FLIP_SAVE_HEALTH_HEALTHY) {
        fprintf(stderr, "FLIP_RECOVERY: RESTORE_PREVIOUS unavailable\n");
        return FLIP_RECOVERY_NO_PREVIOUS;
    }

    /*
     * Persist the resume block before changing *any* save or state generation.
     * Failure is fail-closed: never activate a previous SRAM timeline while
     * the old savestate can still be resumed. Leave the block in place on
     * subsequent failures so a partial restore cannot auto-resume.
     */
    if (flip_recovery_block_resume(recovery_context) < 0) {
        fprintf(stderr, "FLIP_RECOVERY: RESTORE_PREVIOUS failed stage=block-resume\n");
        return FLIP_RECOVERY_ERROR;
    }

    /* Preserve the current state generation before switching cartridge save. */
    if (flip_state_preserve_recovery(state_context) < 0) {
        fprintf(stderr, "FLIP_RECOVERY: RESTORE_PREVIOUS failed stage=state-preserve\n");
        return FLIP_RECOVERY_ERROR;
    }

    /* Preserve current cartridge save and activate protected previous save. */
    if (flip_save_restore_previous(core, save_context) < 0) {
        fprintf(stderr, "FLIP_RECOVERY: RESTORE_PREVIOUS failed stage=save-promote\n");
        return FLIP_RECOVERY_ERROR;
    }

    if (flip_state_quarantine_current(state_context) < 0) {
        fprintf(stderr, "FLIP_RECOVERY: RESTORE_PREVIOUS failed stage=state-quarantine\n");
        return FLIP_RECOVERY_ERROR;
    }

    fprintf(stderr, "FLIP_RECOVERY: RESTORE_PREVIOUS_OK\n");
    return FLIP_RECOVERY_OK;
}

flip_recovery_result_t flip_recovery_undo(
    flip_core_adapter_t *core,
    const flip_save_context_t *save_context,
    const flip_state_context_t *state_context,
    const flip_recovery_context_t *recovery_context)
{
    if (!core || !save_context || !state_context || !recovery_context)
        return FLIP_RECOVERY_ERROR;

    if (flip_save_health(core, save_context, ".sav.recovery") !=
        FLIP_SAVE_HEALTH_HEALTHY) {
        fprintf(stderr, "FLIP_RECOVERY: UNDO unavailable\n");
        return FLIP_RECOVERY_NO_UNDO;
    }

    /* Keep the session blocked while changing timelines. */
    if (flip_recovery_block_resume(recovery_context) < 0) {
        fprintf(stderr, "FLIP_RECOVERY: UNDO failed stage=block-resume\n");
        return FLIP_RECOVERY_ERROR;
    }

    if (flip_save_restore_recovery(core, save_context) < 0) {
        fprintf(stderr, "FLIP_RECOVERY: UNDO failed stage=save-restore\n");
        return FLIP_RECOVERY_ERROR;
    }

    int state_result = flip_state_restore_recovery(state_context);

    if (state_result < 0) {
        (void)flip_state_quarantine_current(state_context);
        fprintf(stderr, "FLIP_RECOVERY: UNDO failed stage=state-restore\n");
        return FLIP_RECOVERY_ERROR;
    }

    if (state_result == 0) {
        /* Save-only undo: never leave a state from the timeline being abandoned. */
        if (flip_state_quarantine_current(state_context) < 0) {
            fprintf(stderr, "FLIP_RECOVERY: UNDO failed stage=state-quarantine\n");
            return FLIP_RECOVERY_ERROR;
        }

        fprintf(stderr, "FLIP_RECOVERY: UNDO_SAVE_ONLY_OK resume=blocked\n");
        return FLIP_RECOVERY_OK;
    }

    if (flip_recovery_clear_resume_block(recovery_context) < 0) {
        fprintf(stderr, "FLIP_RECOVERY: UNDO failed stage=clear-resume-block\n");
        return FLIP_RECOVERY_ERROR;
    }

    fprintf(stderr, "FLIP_RECOVERY: UNDO_PAIRED_OK\n");
    return FLIP_RECOVERY_OK;
}
