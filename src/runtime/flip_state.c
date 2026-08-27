#define _POSIX_C_SOURCE 200809L

#include "flip_state.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static uint64_t flip_state_path_hash(const char *text)
{
    uint64_t hash = UINT64_C(14695981039346656037);

    while (text && *text) {
        hash ^= (unsigned char)*text++;
        hash *= UINT64_C(1099511628211);
    }

    return hash;
}

void flip_state_context_init(
    flip_state_context_t *context,
    const char *system_id,
    const char *rom_path)
{
    if (!context)
        return;

    context->system_id = system_id;
    context->rom_path = rom_path;
    context->current_was_restored = 0;
}

int flip_state_test_build_path(
    const flip_state_context_t *context,
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

    uint64_t hash = flip_state_path_hash(context->rom_path);
    int written = snprintf(
        path,
        path_size,
        FLIP_STATE_ROOT "/%s/%016llx%s",
        context->system_id,
        (unsigned long long)hash,
        suffix);

    if (written < 0 || (size_t)written >= path_size)
        return -1;

    return 0;
}

typedef enum {
    FLIP_STATE_CANDIDATE_MISSING = 0,
    FLIP_STATE_CANDIDATE_RESTORED = 1,
    FLIP_STATE_CANDIDATE_INVALID = 2,
    FLIP_STATE_CANDIDATE_ERROR = 3
} flip_state_candidate_result_t;

static int ensure_directory(const char *path)
{
    if (mkdir(path, 0777) < 0 && errno != EEXIST)
        return -1;

    return 0;
}

static int ensure_state_directories(
    const flip_state_context_t *context)
{
    if (!context || !context->system_id)
        return -1;

    if (ensure_directory(FLIP_STATE_ROOT) < 0)
        return -1;

    char system_dir[512];
    int written = snprintf(
        system_dir,
        sizeof(system_dir),
        FLIP_STATE_ROOT "/%s",
        context->system_id);

    if (written < 0 || (size_t)written >= sizeof(system_dir))
        return -1;

    return ensure_directory(system_dir);
}

static int verify_regular_file_size(
    const char *path,
    size_t expected_size)
{
    struct stat st;

    if (stat(path, &st) < 0)
        return -1;

    if (!S_ISREG(st.st_mode) ||
        st.st_size < 0 ||
        (size_t)st.st_size != expected_size)
        return -1;

    return 0;
}

static int write_atomic_file(
    const char *tmp_path,
    const char *final_path,
    const void *data,
    size_t size)
{
    FILE *file = fopen(tmp_path, "wb");
    if (!file) {
        fprintf(
            stderr,
            "FLIP_STATE: SAVE_FAILED stage=open path=%s error=%s\n",
            tmp_path,
            strerror(errno));
        return -1;
    }

    int ok = fwrite(data, 1, size, file) == size;

    if (ok && fflush(file) != 0)
        ok = 0;

    if (ok && fsync(fileno(file)) < 0)
        ok = 0;

    if (fclose(file) != 0)
        ok = 0;

    if (!ok) {
        fprintf(
            stderr,
            "FLIP_STATE: SAVE_FAILED stage=write path=%s\n",
            tmp_path);
        unlink(tmp_path);
        return -1;
    }

    if (verify_regular_file_size(tmp_path, size) < 0) {
        fprintf(
            stderr,
            "FLIP_STATE: SAVE_FAILED stage=verify path=%s\n",
            tmp_path);
        unlink(tmp_path);
        return -1;
    }

    if (rename(tmp_path, final_path) < 0) {
        fprintf(
            stderr,
            "FLIP_STATE: SAVE_FAILED stage=rename path=%s error=%s\n",
            final_path,
            strerror(errno));
        unlink(tmp_path);
        return -1;
    }

    return 0;
}

static int copy_atomic_file(
    const char *source_path,
    const char *tmp_path,
    const char *final_path,
    size_t expected_size)
{
    FILE *source = fopen(source_path, "rb");
    if (!source)
        return -1;

    void *data = malloc(expected_size);
    if (!data) {
        fclose(source);
        return -1;
    }

    size_t got = fread(data, 1, expected_size, source);
    int extra = fgetc(source);
    int read_error = ferror(source);
    int close_error = fclose(source);

    if (got != expected_size ||
        extra != EOF ||
        read_error ||
        close_error != 0) {
        free(data);
        return -1;
    }

    int result = write_atomic_file(
        tmp_path,
        final_path,
        data,
        expected_size);

    free(data);
    return result;
}

static flip_state_candidate_result_t restore_candidate(
    flip_core_adapter_t *core,
    const char *path,
    size_t expected_size)
{
    struct stat st;

    if (stat(path, &st) < 0) {
        if (errno == ENOENT)
            return FLIP_STATE_CANDIDATE_MISSING;

        fprintf(
            stderr,
            "FLIP_STATE: READ_ERROR path=%s error=%s\n",
            path,
            strerror(errno));
        return FLIP_STATE_CANDIDATE_ERROR;
    }

    if (!S_ISREG(st.st_mode)) {
        fprintf(
            stderr,
            "FLIP_STATE: READ_ERROR path=%s error=not-regular-file\n",
            path);
        return FLIP_STATE_CANDIDATE_ERROR;
    }

    if (st.st_size < 0 || (size_t)st.st_size != expected_size) {
        fprintf(
            stderr,
            "FLIP_STATE: CURRENT_INVALID path=%s reason=size actual=%lld expected=%zu\n",
            path,
            (long long)st.st_size,
            expected_size);
        return FLIP_STATE_CANDIDATE_INVALID;
    }

    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(
            stderr,
            "FLIP_STATE: READ_ERROR path=%s error=%s\n",
            path,
            strerror(errno));
        return FLIP_STATE_CANDIDATE_ERROR;
    }

    void *data = malloc(expected_size);
    if (!data) {
        fclose(file);
        fprintf(
            stderr,
            "FLIP_STATE: READ_ERROR path=%s error=out-of-memory\n",
            path);
        return FLIP_STATE_CANDIDATE_ERROR;
    }

    size_t got = fread(data, 1, expected_size, file);
    int read_error = ferror(file);
    int close_error = fclose(file);

    if (got != expected_size || read_error || close_error != 0) {
        fprintf(
            stderr,
            "FLIP_STATE: READ_ERROR path=%s got=%zu expected=%zu\n",
            path,
            got,
            expected_size);
        free(data);
        return FLIP_STATE_CANDIDATE_ERROR;
    }

    int restored = flip_core_unserialize(core, data, expected_size);
    free(data);

    if (restored < 0) {
        fprintf(
            stderr,
            "FLIP_STATE: CURRENT_INVALID path=%s reason=unserialize\n",
            path);
        return FLIP_STATE_CANDIDATE_INVALID;
    }

    return FLIP_STATE_CANDIDATE_RESTORED;
}

int flip_state_save(
    flip_core_adapter_t *core,
    flip_state_context_t *context)
{
    if (!core || !context)
        return -1;

    size_t size = flip_core_serialize_size(core);
    if (size == 0) {
        fprintf(stderr, "FLIP_STATE: SAVE_FAILED stage=serialize-size\n");
        return -1;
    }

    void *data = malloc(size);
    if (!data) {
        fprintf(stderr, "FLIP_STATE: SAVE_FAILED stage=allocate\n");
        return -1;
    }

    if (flip_core_serialize(core, data, size) < 0) {
        fprintf(stderr, "FLIP_STATE: SAVE_FAILED stage=serialize\n");
        free(data);
        return -1;
    }

    if (ensure_state_directories(context) < 0) {
        fprintf(
            stderr,
            "FLIP_STATE: SAVE_FAILED stage=mkdir error=%s\n",
            strerror(errno));
        free(data);
        return -1;
    }

    char current_path[512];
    char tmp_path[512];
    char previous_path[512];
    char previous_tmp_path[512];

    if (flip_state_test_build_path(
            context,
            ".state",
            current_path,
            sizeof(current_path)) < 0 ||
        flip_state_test_build_path(
            context,
            ".state.tmp",
            tmp_path,
            sizeof(tmp_path)) < 0 ||
        flip_state_test_build_path(
            context,
            ".state.prev",
            previous_path,
            sizeof(previous_path)) < 0 ||
        flip_state_test_build_path(
            context,
            ".state.prev.tmp",
            previous_tmp_path,
            sizeof(previous_tmp_path)) < 0) {
        free(data);
        return -1;
    }

    if (context->current_was_restored) {
        if (verify_regular_file_size(current_path, size) < 0 ||
            copy_atomic_file(
                current_path,
                previous_tmp_path,
                previous_path,
                size) < 0) {
            fprintf(stderr, "FLIP_STATE: SAVE_FAILED stage=preserve-previous\n");
            free(data);
            return -1;
        }
    }

    if (write_atomic_file(
            tmp_path,
            current_path,
            data,
            size) < 0) {
        free(data);
        return -1;
    }

    free(data);
    context->current_was_restored = 0;

    fprintf(
        stderr,
        "FLIP_STATE: SAVE_OK path=%s size=%zu\n",
        current_path,
        size);

    return 0;
}

flip_state_restore_result_t flip_state_restore(
    flip_core_adapter_t *core,
    flip_state_context_t *context)
{
    if (!core || !context)
        return FLIP_STATE_RESTORE_ERROR;

    context->current_was_restored = 0;

    size_t expected_size = flip_core_serialize_size(core);
    if (expected_size == 0) {
        fprintf(stderr, "FLIP_STATE: RESTORE_UNUSABLE reason=serialize-size-zero\n");
        return FLIP_STATE_RESTORE_UNUSABLE;
    }

    char current_path[512];
    char previous_path[512];

    if (flip_state_test_build_path(
            context,
            ".state",
            current_path,
            sizeof(current_path)) < 0 ||
        flip_state_test_build_path(
            context,
            ".state.prev",
            previous_path,
            sizeof(previous_path)) < 0) {
        return FLIP_STATE_RESTORE_ERROR;
    }

    flip_state_candidate_result_t current = restore_candidate(
        core,
        current_path,
        expected_size);

    if (current == FLIP_STATE_CANDIDATE_ERROR)
        return FLIP_STATE_RESTORE_ERROR;

    if (current == FLIP_STATE_CANDIDATE_RESTORED) {
        context->current_was_restored = 1;
        fprintf(
            stderr,
            "FLIP_STATE: RESTORE_CURRENT_OK path=%s size=%zu\n",
            current_path,
            expected_size);
        return FLIP_STATE_RESTORE_CURRENT;
    }

    flip_state_candidate_result_t previous = restore_candidate(
        core,
        previous_path,
        expected_size);

    if (previous == FLIP_STATE_CANDIDATE_ERROR)
        return FLIP_STATE_RESTORE_ERROR;

    if (previous == FLIP_STATE_CANDIDATE_RESTORED) {
        fprintf(
            stderr,
            "FLIP_STATE: RESTORE_PREVIOUS_OK path=%s size=%zu\n",
            previous_path,
            expected_size);
        return FLIP_STATE_RESTORE_PREVIOUS;
    }

    if (current == FLIP_STATE_CANDIDATE_MISSING &&
        previous == FLIP_STATE_CANDIDATE_MISSING) {
        fprintf(stderr, "FLIP_STATE: NO_STATE\n");
        return FLIP_STATE_RESTORE_NONE;
    }

    fprintf(stderr, "FLIP_STATE: RESTORE_UNUSABLE\n");
    return FLIP_STATE_RESTORE_UNUSABLE;
}

int flip_state_preserve_recovery(
    const flip_state_context_t *context)
{
    if (!context || !context->system_id || !context->rom_path)
        return -1;

    if (ensure_state_directories(context) < 0)
        return -1;

    char current_path[512];
    char recovery_path[512];
    char recovery_tmp_path[512];

    if (flip_state_test_build_path(
            context,
            ".state",
            current_path,
            sizeof(current_path)) < 0 ||
        flip_state_test_build_path(
            context,
            ".state.recovery",
            recovery_path,
            sizeof(recovery_path)) < 0 ||
        flip_state_test_build_path(
            context,
            ".state.recovery.tmp",
            recovery_tmp_path,
            sizeof(recovery_tmp_path)) < 0) {
        return -1;
    }

    struct stat st;
    if (stat(current_path, &st) < 0) {
        if (errno != ENOENT)
            return -1;

        if (unlink(recovery_path) < 0 && errno != ENOENT)
            return -1;

        if (unlink(recovery_tmp_path) < 0 && errno != ENOENT)
            return -1;

        fprintf(stderr, "FLIP_STATE: RECOVERY_NONE\n");
        return 0;
    }

    if (!S_ISREG(st.st_mode) || st.st_size <= 0)
        return -1;

    size_t size = (size_t)st.st_size;

    if (copy_atomic_file(
            current_path,
            recovery_tmp_path,
            recovery_path,
            size) < 0) {
        fprintf(stderr, "FLIP_STATE: RECOVERY_FAILED stage=preserve\n");
        return -1;
    }

    fprintf(
        stderr,
        "FLIP_STATE: RECOVERY_OK path=%s size=%zu\n",
        recovery_path,
        size);

    return 1;
}

int flip_state_quarantine_current(
    const flip_state_context_t *context)
{
    if (!context || !context->system_id || !context->rom_path)
        return -1;

    char current_path[512];

    if (flip_state_test_build_path(
            context,
            ".state",
            current_path,
            sizeof(current_path)) < 0) {
        return -1;
    }

    if (unlink(current_path) < 0 && errno != ENOENT) {
        fprintf(stderr, "FLIP_STATE: QUARANTINE_FAILED path=%s\n", current_path);
        return -1;
    }

    fprintf(stderr, "FLIP_STATE: QUARANTINE_OK path=%s\n", current_path);
    return 0;
}

int flip_state_restore_recovery(
    const flip_state_context_t *context)
{
    if (!context || !context->system_id || !context->rom_path)
        return -1;

    if (ensure_state_directories(context) < 0)
        return -1;

    char current_path[512];
    char current_tmp_path[512];
    char recovery_path[512];

    if (flip_state_test_build_path(
            context,
            ".state",
            current_path,
            sizeof(current_path)) < 0 ||
        flip_state_test_build_path(
            context,
            ".state.tmp",
            current_tmp_path,
            sizeof(current_tmp_path)) < 0 ||
        flip_state_test_build_path(
            context,
            ".state.recovery",
            recovery_path,
            sizeof(recovery_path)) < 0) {
        return -1;
    }

    struct stat st;
    if (stat(recovery_path, &st) < 0) {
        if (errno == ENOENT)
            return 0;
        return -1;
    }

    if (!S_ISREG(st.st_mode) || st.st_size <= 0)
        return 0;

    size_t size = (size_t)st.st_size;

    if (copy_atomic_file(
            recovery_path,
            current_tmp_path,
            current_path,
            size) < 0) {
        fprintf(stderr, "FLIP_STATE: RECOVERY_RESTORE_FAILED\n");
        return -1;
    }

    fprintf(
        stderr,
        "FLIP_STATE: RECOVERY_RESTORE_OK path=%s size=%zu\n",
        current_path,
        size);

    return 1;
}
