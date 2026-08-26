#define _POSIX_C_SOURCE 200809L

#include "flip_save.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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
    (void)core;
    (void)context;
    return -1;
}
