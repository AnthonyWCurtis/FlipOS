#define _POSIX_C_SOURCE 200809L

#include "flip_save.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static uint64_t flip_save_path_hash(const char *text)
{
    uint64_t hash = UINT64_C(14695981039346656037);

    while (text && *text) {
        hash ^= (unsigned char)*text++;
        hash *= UINT64_C(1099511628211);
    }

    return hash;
}

void flip_save_context_init(
    flip_save_context_t *context,
    const char *system_id,
    const char *rom_path)
{
    if (!context)
        return;

    context->system_id = system_id;
    context->rom_path = rom_path;
}

int flip_save_test_build_path(
    const flip_save_context_t *context,
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

    uint64_t hash = flip_save_path_hash(context->rom_path);
    int written = snprintf(
        path,
        path_size,
        FLIP_SAVE_ROOT "/%s/%016llx%s",
        context->system_id,
        (unsigned long long)hash,
        suffix);

    if (written < 0 || (size_t)written >= path_size)
        return -1;

    return 0;
}

typedef enum {
    FLIP_SAVE_CANDIDATE_MISSING = 0,
    FLIP_SAVE_CANDIDATE_VALID = 1,
    FLIP_SAVE_CANDIDATE_INVALID = 2,
    FLIP_SAVE_CANDIDATE_ERROR = 3
} flip_save_candidate_result_t;

static int ensure_directory(const char *path)
{
    if (mkdir(path, 0777) < 0 && errno != EEXIST)
        return -1;

    return 0;
}

static int ensure_save_directories(const flip_save_context_t *context)
{
    if (!context || !context->system_id)
        return -1;

    if (ensure_directory(FLIP_SAVE_ROOT) < 0)
        return -1;

    char system_dir[512];
    int written = snprintf(
        system_dir,
        sizeof(system_dir),
        FLIP_SAVE_ROOT "/%s",
        context->system_id);

    if (written < 0 || (size_t)written >= sizeof(system_dir))
        return -1;

    return ensure_directory(system_dir);
}

static int verify_regular_file_size(const char *path, size_t expected_size)
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

static int write_synced_file(
    const char *path,
    const void *data,
    size_t size)
{
    FILE *file = fopen(path, "wb");
    if (!file)
        return -1;

    int ok = fwrite(data, 1, size, file) == size;

    if (ok && fflush(file) != 0)
        ok = 0;

    if (ok && fsync(fileno(file)) < 0)
        ok = 0;

    if (fclose(file) != 0)
        ok = 0;

    if (!ok || verify_regular_file_size(path, size) < 0) {
        unlink(path);
        return -1;
    }

    return 0;
}

static int copy_synced_file(
    const char *source_path,
    const char *tmp_path,
    const char *final_path,
    size_t size)
{
    unsigned char *buffer = malloc(size);
    if (!buffer)
        return -1;

    FILE *source = fopen(source_path, "rb");
    if (!source) {
        free(buffer);
        return -1;
    }

    int ok = fread(buffer, 1, size, source) == size;
    if (ok && fgetc(source) != EOF)
        ok = 0;
    if (fclose(source) != 0)
        ok = 0;

    if (!ok) {
        free(buffer);
        return -1;
    }

    if (write_synced_file(tmp_path, buffer, size) < 0) {
        free(buffer);
        return -1;
    }

    free(buffer);

    if (rename(tmp_path, final_path) < 0) {
        unlink(tmp_path);
        return -1;
    }

    return 0;
}

static flip_save_candidate_result_t inspect_candidate(
    const char *path,
    size_t expected_size)
{
    struct stat st;

    if (stat(path, &st) < 0) {
        if (errno == ENOENT)
            return FLIP_SAVE_CANDIDATE_MISSING;
        return FLIP_SAVE_CANDIDATE_ERROR;
    }

    if (!S_ISREG(st.st_mode) ||
        st.st_size < 0 ||
        (size_t)st.st_size != expected_size)
        return FLIP_SAVE_CANDIDATE_INVALID;

    unsigned char *buffer = malloc(expected_size);
    if (!buffer)
        return FLIP_SAVE_CANDIDATE_ERROR;

    FILE *file = fopen(path, "rb");
    if (!file) {
        free(buffer);
        return FLIP_SAVE_CANDIDATE_ERROR;
    }

    int ok = fread(buffer, 1, expected_size, file) == expected_size;
    if (ok && fgetc(file) != EOF)
        ok = 0;
    if (fclose(file) != 0)
        ok = 0;

    free(buffer);
    return ok ? FLIP_SAVE_CANDIDATE_VALID : FLIP_SAVE_CANDIDATE_ERROR;
}

static flip_save_candidate_result_t load_candidate(
    const char *path,
    void *memory,
    size_t memory_size)
{
    struct stat st;

    if (stat(path, &st) < 0) {
        if (errno == ENOENT)
            return FLIP_SAVE_CANDIDATE_MISSING;

        return FLIP_SAVE_CANDIDATE_ERROR;
    }

    if (!S_ISREG(st.st_mode) ||
        st.st_size < 0 ||
        (size_t)st.st_size != memory_size)
        return FLIP_SAVE_CANDIDATE_INVALID;

    unsigned char *buffer = malloc(memory_size);
    if (!buffer)
        return FLIP_SAVE_CANDIDATE_ERROR;

    FILE *file = fopen(path, "rb");
    if (!file) {
        free(buffer);
        return FLIP_SAVE_CANDIDATE_ERROR;
    }

    int ok = fread(buffer, 1, memory_size, file) == memory_size;
    if (ok && fgetc(file) != EOF)
        ok = 0;
    if (fclose(file) != 0)
        ok = 0;

    if (!ok) {
        free(buffer);
        return FLIP_SAVE_CANDIDATE_ERROR;
    }

    memcpy(memory, buffer, memory_size);
    free(buffer);
    return FLIP_SAVE_CANDIDATE_VALID;
}

flip_save_health_t flip_save_health(
    flip_core_adapter_t *core,
    const flip_save_context_t *context,
    const char *suffix)
{
    if (!core || !context || !suffix)
        return FLIP_SAVE_HEALTH_ERROR;

    size_t memory_size = flip_core_save_memory_size(core);
    if (memory_size == 0)
        return FLIP_SAVE_HEALTH_ERROR;

    char path[512];
    if (flip_save_test_build_path(context, suffix, path, sizeof(path)) < 0)
        return FLIP_SAVE_HEALTH_ERROR;

    switch (inspect_candidate(path, memory_size)) {
    case FLIP_SAVE_CANDIDATE_MISSING:
        return FLIP_SAVE_HEALTH_MISSING;
    case FLIP_SAVE_CANDIDATE_VALID:
        return FLIP_SAVE_HEALTH_HEALTHY;
    case FLIP_SAVE_CANDIDATE_INVALID:
        return FLIP_SAVE_HEALTH_INVALID;
    case FLIP_SAVE_CANDIDATE_ERROR:
    default:
        return FLIP_SAVE_HEALTH_ERROR;
    }
}

int flip_save_query_status(
    flip_core_adapter_t *core,
    const flip_save_context_t *context,
    flip_save_status_t *status)
{
    if (!core || !context || !status)
        return -1;

    status->current =
        flip_save_health(
            core,
            context,
            FLIP_SAVE_CURRENT_SUFFIX);

    status->previous =
        flip_save_health(
            core,
            context,
            FLIP_SAVE_PREVIOUS_SUFFIX);

    status->recovery =
        flip_save_health(
            core,
            context,
            FLIP_SAVE_RECOVERY_SUFFIX);

    return 0;
}

flip_save_load_result_t flip_save_load(
    flip_core_adapter_t *core,
    const flip_save_context_t *context)
{
    if (!core || !context || !context->system_id || !context->rom_path)
        return FLIP_SAVE_LOAD_ERROR;

    void *memory = flip_core_save_memory_data(core);
    size_t memory_size = flip_core_save_memory_size(core);

    if (!memory || memory_size == 0) {
        fprintf(stderr, "FLIP_SAVE: LOAD_NO_MEMORY\n");
        return FLIP_SAVE_LOAD_NO_MEMORY;
    }

    char current_path[512];
    char previous_path[512];

    if (flip_save_test_build_path(
            context,
            ".sav",
            current_path,
            sizeof(current_path)) < 0 ||
        flip_save_test_build_path(
            context,
            ".sav.prev",
            previous_path,
            sizeof(previous_path)) < 0) {
        return FLIP_SAVE_LOAD_ERROR;
    }

    flip_save_candidate_result_t current =
        load_candidate(current_path, memory, memory_size);

    if (current == FLIP_SAVE_CANDIDATE_VALID) {
        fprintf(stderr, "FLIP_SAVE: LOAD_CURRENT_OK size=%zu\n", memory_size);
        return FLIP_SAVE_LOAD_CURRENT;
    }

    flip_save_candidate_result_t previous =
        load_candidate(previous_path, memory, memory_size);

    if (previous == FLIP_SAVE_CANDIDATE_VALID) {
        fprintf(stderr, "FLIP_SAVE: LOAD_PREVIOUS_OK size=%zu\n", memory_size);
        return FLIP_SAVE_LOAD_PREVIOUS;
    }

    if (current == FLIP_SAVE_CANDIDATE_ERROR ||
        previous == FLIP_SAVE_CANDIDATE_ERROR) {
        fprintf(stderr, "FLIP_SAVE: LOAD_ERROR\n");
        return FLIP_SAVE_LOAD_ERROR;
    }

    if (current == FLIP_SAVE_CANDIDATE_MISSING &&
        previous == FLIP_SAVE_CANDIDATE_MISSING) {
        fprintf(stderr, "FLIP_SAVE: LOAD_NO_SAVE\n");
        return FLIP_SAVE_LOAD_NO_SAVE;
    }

    fprintf(stderr, "FLIP_SAVE: LOAD_UNUSABLE\n");
    return FLIP_SAVE_LOAD_UNUSABLE;
}

int flip_save_commit(
    flip_core_adapter_t *core,
    const flip_save_context_t *context)
{
    if (!core || !context || !context->system_id || !context->rom_path)
        return -1;

    void *memory = flip_core_save_memory_data(core);
    size_t memory_size = flip_core_save_memory_size(core);

    if (!memory || memory_size == 0) {
        fprintf(stderr, "FLIP_SAVE: SAVE_NO_MEMORY\n");
        return 0;
    }

    if (ensure_save_directories(context) < 0) {
        fprintf(stderr, "FLIP_SAVE: SAVE_FAILED stage=mkdir\n");
        return -1;
    }

    char current_path[512];
    char previous_path[512];
    char current_tmp_path[512];
    char previous_tmp_path[512];

    if (flip_save_test_build_path(context, ".sav", current_path, sizeof(current_path)) < 0 ||
        flip_save_test_build_path(context, ".sav.prev", previous_path, sizeof(previous_path)) < 0 ||
        flip_save_test_build_path(context, ".sav.tmp", current_tmp_path, sizeof(current_tmp_path)) < 0 ||
        flip_save_test_build_path(context, ".sav.prev.tmp", previous_tmp_path, sizeof(previous_tmp_path)) < 0) {
        fprintf(stderr, "FLIP_SAVE: SAVE_FAILED stage=path\n");
        return -1;
    }

    if (write_synced_file(current_tmp_path, memory, memory_size) < 0) {
        fprintf(
            stderr,
            "FLIP_SAVE: SAVE_FAILED stage=write path=%s error=%s\n",
            current_tmp_path,
            strerror(errno));
        return -1;
    }

    int current_is_valid =
        verify_regular_file_size(current_path, memory_size) == 0;

    if (current_is_valid) {
        if (copy_synced_file(
                current_path,
                previous_tmp_path,
                previous_path,
                memory_size) < 0) {
            fprintf(stderr, "FLIP_SAVE: SAVE_FAILED stage=previous\n");
            unlink(current_tmp_path);
            return -1;
        }
    }

    if (rename(current_tmp_path, current_path) < 0) {
        fprintf(
            stderr,
            "FLIP_SAVE: SAVE_FAILED stage=rename path=%s error=%s\n",
            current_path,
            strerror(errno));
        unlink(current_tmp_path);
        return -1;
    }

    fprintf(stderr, "FLIP_SAVE: SAVE_OK path=%s size=%zu\n", current_path, memory_size);
    return 0;
}

int flip_save_restore_previous(
    flip_core_adapter_t *core,
    const flip_save_context_t *context)
{
    if (!core || !context)
        return -1;

    size_t memory_size = flip_core_save_memory_size(core);
    if (memory_size == 0 || ensure_save_directories(context) < 0)
        return -1;

    char current_path[512];
    char previous_path[512];
    char current_tmp_path[512];
    char recovery_path[512];
    char recovery_tmp_path[512];

    if (flip_save_test_build_path(context, ".sav", current_path, sizeof(current_path)) < 0 ||
        flip_save_test_build_path(context, ".sav.prev", previous_path, sizeof(previous_path)) < 0 ||
        flip_save_test_build_path(context, ".sav.tmp", current_tmp_path, sizeof(current_tmp_path)) < 0 ||
        flip_save_test_build_path(context, ".sav.recovery", recovery_path, sizeof(recovery_path)) < 0 ||
        flip_save_test_build_path(context, ".sav.recovery.tmp", recovery_tmp_path, sizeof(recovery_tmp_path)) < 0)
        return -1;

    if (inspect_candidate(previous_path, memory_size) != FLIP_SAVE_CANDIDATE_VALID)
        return -1;

    flip_save_candidate_result_t current = inspect_candidate(current_path, memory_size);
    if (current == FLIP_SAVE_CANDIDATE_INVALID || current == FLIP_SAVE_CANDIDATE_ERROR)
        return -1;

    if (current == FLIP_SAVE_CANDIDATE_VALID) {
        if (copy_synced_file(
                current_path,
                recovery_tmp_path,
                recovery_path,
                memory_size) < 0)
            return -1;
    } else {
        unlink(recovery_tmp_path);
        unlink(recovery_path);
    }

    if (copy_synced_file(
            previous_path,
            current_tmp_path,
            current_path,
            memory_size) < 0)
        return -1;

    fprintf(stderr, "FLIP_SAVE: RESTORE_PREVIOUS_OK size=%zu\n", memory_size);
    return 0;
}

int flip_save_restore_recovery(
    flip_core_adapter_t *core,
    const flip_save_context_t *context)
{
    if (!core || !context)
        return -1;

    size_t memory_size = flip_core_save_memory_size(core);
    if (memory_size == 0 || ensure_save_directories(context) < 0)
        return -1;

    char current_path[512];
    char previous_path[512];
    char current_tmp_path[512];
    char previous_tmp_path[512];
    char recovery_path[512];

    if (flip_save_test_build_path(context, ".sav", current_path, sizeof(current_path)) < 0 ||
        flip_save_test_build_path(context, ".sav.prev", previous_path, sizeof(previous_path)) < 0 ||
        flip_save_test_build_path(context, ".sav.tmp", current_tmp_path, sizeof(current_tmp_path)) < 0 ||
        flip_save_test_build_path(context, ".sav.prev.tmp", previous_tmp_path, sizeof(previous_tmp_path)) < 0 ||
        flip_save_test_build_path(context, ".sav.recovery", recovery_path, sizeof(recovery_path)) < 0)
        return -1;

    if (inspect_candidate(recovery_path, memory_size) != FLIP_SAVE_CANDIDATE_VALID)
        return -1;

    flip_save_candidate_result_t current = inspect_candidate(current_path, memory_size);
    if (current == FLIP_SAVE_CANDIDATE_INVALID || current == FLIP_SAVE_CANDIDATE_ERROR)
        return -1;

    if (current == FLIP_SAVE_CANDIDATE_VALID) {
        if (copy_synced_file(
                current_path,
                previous_tmp_path,
                previous_path,
                memory_size) < 0)
            return -1;
    }

    if (copy_synced_file(
            recovery_path,
            current_tmp_path,
            current_path,
            memory_size) < 0)
        return -1;

    fprintf(stderr, "FLIP_SAVE: RESTORE_RECOVERY_OK size=%zu\n", memory_size);
    return 0;
}
