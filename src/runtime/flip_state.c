#include "flip_state.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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
