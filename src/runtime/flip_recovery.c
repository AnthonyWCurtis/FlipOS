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
