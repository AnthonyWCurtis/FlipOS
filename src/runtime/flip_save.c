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

int flip_save_test_build_legacy_path(
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

static int flip_save_rom_stem(
    const flip_save_context_t *context,
    char *stem,
    size_t stem_size)
{
    if (!context || !context->rom_path || !stem || stem_size == 0)
        return -1;

    const char *name = strrchr(context->rom_path, '/');
    name = name ? name + 1 : context->rom_path;
    if (!*name)
        return -1;

    const char *dot = strrchr(name, '.');
    size_t length =
        dot && dot != name
            ? (size_t)(dot - name)
            : strlen(name);

    if (length == 0 || length >= stem_size)
        return -1;

    memcpy(stem, name, length);
    stem[length] = '\0';
    return 0;
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

    char stem[384];
    if (flip_save_rom_stem(context, stem, sizeof(stem)) < 0)
        return -1;

    int written = snprintf(
        path,
        path_size,
        FLIP_SAVE_ROOT "/%s/%s%s",
        context->system_id,
        stem,
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

static int gba_resolved_save_size(size_t size)
{
    switch (size) {
    case 0x00000200: /* EEPROM 512 B */
    case 0x00002000: /* EEPROM 8 KiB */
    case 0x00008000: /* SRAM 32 KiB */
    case 0x00010000: /* SRAM/FLASH 64 KiB */
    case 0x00020000: /* FLASH 128 KiB */
        return 1;
    default:
        return 0;
    }
}

static int candidate_size_compatible(
    const flip_save_context_t *context,
    size_t file_size,
    size_t memory_size)
{
    if (file_size == memory_size)
        return 1;

    /*
     * mGBA reports the 128 KiB FLASH1M maximum while GBA save type is still
     * AUTODETECT. After the game runs, the same core can resolve that region
     * to EEPROM/SRAM/FLASH and report a smaller canonical save size.
     *
     * Accept only those known GBA sizes, and only while the live core still
     * exposes the 128 KiB autodetect region. Arbitrary truncation remains
     * invalid.
     */
    return context &&
        context->system_id &&
        strcmp(context->system_id, "gba") == 0 &&
        memory_size == 0x00020000 &&
        gba_resolved_save_size(file_size);
}

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

static int migrate_legacy_path(
    const flip_save_context_t *context,
    const char *suffix)
{
    char readable_path[512];
    char legacy_path[512];
    struct stat st;

    if (flip_save_test_build_path(
            context,
            suffix,
            readable_path,
            sizeof(readable_path)) < 0 ||
        flip_save_test_build_legacy_path(
            context,
            suffix,
            legacy_path,
            sizeof(legacy_path)) < 0)
        return -1;

    if (stat(readable_path, &st) == 0)
        return 0;
    if (errno != ENOENT)
        return -1;

    if (stat(legacy_path, &st) < 0)
        return errno == ENOENT ? 0 : -1;
    if (!S_ISREG(st.st_mode))
        return -1;

    if (ensure_save_directories(context) < 0)
        return -1;

    if (rename(legacy_path, readable_path) < 0)
        return -1;

    fprintf(
        stderr,
        "FLIP_SAVE: MIGRATED_LEGACY from=%s to=%s\n",
        legacy_path,
        readable_path);
    return 0;
}

static int migrate_legacy_save_files(const flip_save_context_t *context)
{
    static const char *const suffixes[] = {
        FLIP_SAVE_CURRENT_SUFFIX,
        FLIP_SAVE_PREVIOUS_SUFFIX,
        FLIP_SAVE_RECOVERY_SUFFIX,
        FLIP_RTC_CURRENT_SUFFIX,
        FLIP_RTC_PREVIOUS_SUFFIX,
    };

    for (size_t i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); ++i) {
        if (migrate_legacy_path(context, suffixes[i]) < 0)
            return -1;
    }

    return 0;
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
    const flip_save_context_t *context,
    size_t expected_size,
    size_t *actual_size)
{
    struct stat st;

    if (actual_size)
        *actual_size = 0;

    if (stat(path, &st) < 0) {
        if (errno == ENOENT)
            return FLIP_SAVE_CANDIDATE_MISSING;
        return FLIP_SAVE_CANDIDATE_ERROR;
    }

    if (!S_ISREG(st.st_mode) ||
        st.st_size <= 0 ||
        !candidate_size_compatible(
            context,
            (size_t)st.st_size,
            expected_size))
        return FLIP_SAVE_CANDIDATE_INVALID;

    size_t file_size = (size_t)st.st_size;
    unsigned char *buffer = malloc(file_size);
    if (!buffer)
        return FLIP_SAVE_CANDIDATE_ERROR;

    FILE *file = fopen(path, "rb");
    if (!file) {
        free(buffer);
        return FLIP_SAVE_CANDIDATE_ERROR;
    }

    int ok = fread(buffer, 1, file_size, file) == file_size;
    if (ok && fgetc(file) != EOF)
        ok = 0;
    if (fclose(file) != 0)
        ok = 0;

    free(buffer);

    if (!ok)
        return FLIP_SAVE_CANDIDATE_ERROR;

    if (actual_size)
        *actual_size = file_size;

    return FLIP_SAVE_CANDIDATE_VALID;
}

static flip_save_candidate_result_t load_candidate(
    const char *path,
    const flip_save_context_t *context,
    void *memory,
    size_t memory_size,
    size_t *loaded_size)
{
    size_t file_size = 0;
    flip_save_candidate_result_t inspected =
        inspect_candidate(
            path,
            context,
            memory_size,
            &file_size);

    if (inspected != FLIP_SAVE_CANDIDATE_VALID)
        return inspected;

    unsigned char *buffer = malloc(file_size);
    if (!buffer)
        return FLIP_SAVE_CANDIDATE_ERROR;

    FILE *file = fopen(path, "rb");
    if (!file) {
        free(buffer);
        return FLIP_SAVE_CANDIDATE_ERROR;
    }

    int ok = fread(buffer, 1, file_size, file) == file_size;
    if (ok && fgetc(file) != EOF)
        ok = 0;
    if (fclose(file) != 0)
        ok = 0;

    if (!ok) {
        free(buffer);
        return FLIP_SAVE_CANDIDATE_ERROR;
    }

    memcpy(memory, buffer, file_size);
    free(buffer);

    if (loaded_size)
        *loaded_size = file_size;

    return FLIP_SAVE_CANDIDATE_VALID;
}

flip_save_health_t flip_save_health(
    flip_core_adapter_t *core,
    const flip_save_context_t *context,
    const char *suffix)
{
    if (!core || !context || !suffix)
        return FLIP_SAVE_HEALTH_ERROR;

    if (migrate_legacy_save_files(context) < 0)
        return FLIP_SAVE_HEALTH_ERROR;

    size_t memory_size = flip_core_save_memory_size(core);
    if (memory_size == 0)
        return FLIP_SAVE_HEALTH_ERROR;

    char path[512];
    if (flip_save_test_build_path(context, suffix, path, sizeof(path)) < 0)
        return FLIP_SAVE_HEALTH_ERROR;

    switch (inspect_candidate(path, context, memory_size, NULL)) {
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

    if (migrate_legacy_save_files(context) < 0)
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

    size_t current_size = 0;
    flip_save_candidate_result_t current =
        load_candidate(
            current_path,
            context,
            memory,
            memory_size,
            &current_size);

    if (current == FLIP_SAVE_CANDIDATE_VALID) {
        fprintf(
            stderr,
            "FLIP_SAVE: LOAD_CURRENT_OK size=%zu memory_size=%zu\n",
            current_size,
            memory_size);
        return FLIP_SAVE_LOAD_CURRENT;
    }

    size_t previous_size = 0;
    flip_save_candidate_result_t previous =
        load_candidate(
            previous_path,
            context,
            memory,
            memory_size,
            &previous_size);

    if (previous == FLIP_SAVE_CANDIDATE_VALID) {
        fprintf(
            stderr,
            "FLIP_SAVE: LOAD_PREVIOUS_OK size=%zu memory_size=%zu\n",
            previous_size,
            memory_size);
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

    if (migrate_legacy_save_files(context) < 0)
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

    size_t current_size = 0;
    int current_is_valid =
        inspect_candidate(
            current_path,
            context,
            memory_size,
            &current_size) == FLIP_SAVE_CANDIDATE_VALID;

    if (current_is_valid) {
        if (copy_synced_file(
                current_path,
                previous_tmp_path,
                previous_path,
                current_size) < 0) {
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

    if (migrate_legacy_save_files(context) < 0)
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

    size_t previous_size = 0;
    if (inspect_candidate(
            previous_path,
            context,
            memory_size,
            &previous_size) != FLIP_SAVE_CANDIDATE_VALID)
        return -1;

    size_t current_size = 0;
    flip_save_candidate_result_t current =
        inspect_candidate(
            current_path,
            context,
            memory_size,
            &current_size);
    if (current == FLIP_SAVE_CANDIDATE_INVALID || current == FLIP_SAVE_CANDIDATE_ERROR)
        return -1;

    if (current == FLIP_SAVE_CANDIDATE_VALID) {
        if (copy_synced_file(
                current_path,
                recovery_tmp_path,
                recovery_path,
                current_size) < 0)
            return -1;
    } else {
        unlink(recovery_tmp_path);
        unlink(recovery_path);
    }

    if (copy_synced_file(
            previous_path,
            current_tmp_path,
            current_path,
            previous_size) < 0)
        return -1;

    fprintf(
        stderr,
        "FLIP_SAVE: RESTORE_PREVIOUS_OK size=%zu memory_size=%zu\n",
        previous_size,
        memory_size);
    return 0;
}

int flip_save_restore_recovery(
    flip_core_adapter_t *core,
    const flip_save_context_t *context)
{
    if (!core || !context)
        return -1;

    if (migrate_legacy_save_files(context) < 0)
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

    size_t recovery_size = 0;
    if (inspect_candidate(
            recovery_path,
            context,
            memory_size,
            &recovery_size) != FLIP_SAVE_CANDIDATE_VALID)
        return -1;

    size_t current_size = 0;
    flip_save_candidate_result_t current =
        inspect_candidate(
            current_path,
            context,
            memory_size,
            &current_size);
    if (current == FLIP_SAVE_CANDIDATE_INVALID || current == FLIP_SAVE_CANDIDATE_ERROR)
        return -1;

    if (current == FLIP_SAVE_CANDIDATE_VALID) {
        if (copy_synced_file(
                current_path,
                previous_tmp_path,
                previous_path,
                current_size) < 0)
            return -1;
    }

    if (copy_synced_file(
            recovery_path,
            current_tmp_path,
            current_path,
            recovery_size) < 0)
        return -1;

    fprintf(
        stderr,
        "FLIP_SAVE: RESTORE_RECOVERY_OK size=%zu memory_size=%zu\n",
        recovery_size,
        memory_size);
    return 0;
}
static int load_exact_candidate(
    const char *path,
    void *memory,
    size_t memory_size)
{
    struct stat st;
    if (stat(path, &st) < 0)
        return errno == ENOENT ? 0 : -1;

    if (!S_ISREG(st.st_mode) ||
        st.st_size < 0 ||
        (size_t)st.st_size != memory_size)
        return -1;

    FILE *file = fopen(path, "rb");
    if (!file)
        return -1;

    int ok = fread(memory, 1, memory_size, file) == memory_size;
    if (ok && fgetc(file) != EOF)
        ok = 0;
    if (fclose(file) != 0)
        ok = 0;

    return ok ? 1 : -1;
}

int flip_rtc_load(
    flip_core_adapter_t *core,
    const flip_save_context_t *context)
{
    if (!core || !context || !context->system_id || !context->rom_path)
        return -1;

    if (migrate_legacy_save_files(context) < 0)
        return -1;

    void *memory = flip_core_rtc_memory_data(core);
    size_t memory_size = flip_core_rtc_memory_size(core);

    if (!memory || memory_size == 0) {
        fprintf(stderr, "FLIP_RTC: LOAD_NO_MEMORY\n");
        return 0;
    }

    char current_path[512];
    char previous_path[512];

    if (flip_save_test_build_path(
            context,
            FLIP_RTC_CURRENT_SUFFIX,
            current_path,
            sizeof(current_path)) < 0 ||
        flip_save_test_build_path(
            context,
            FLIP_RTC_PREVIOUS_SUFFIX,
            previous_path,
            sizeof(previous_path)) < 0)
        return -1;

    int current = load_exact_candidate(
        current_path,
        memory,
        memory_size);
    if (current == 1) {
        fprintf(
            stderr,
            "FLIP_RTC: LOAD_CURRENT_OK path=%s size=%zu\n",
            current_path,
            memory_size);
        return 0;
    }

    int previous = load_exact_candidate(
        previous_path,
        memory,
        memory_size);
    if (previous == 1) {
        fprintf(
            stderr,
            "FLIP_RTC: LOAD_PREVIOUS_OK path=%s size=%zu\n",
            previous_path,
            memory_size);
        return 0;
    }

    if (current < 0 || previous < 0) {
        fprintf(stderr, "FLIP_RTC: LOAD_INVALID\n");
        return -1;
    }

    fprintf(stderr, "FLIP_RTC: LOAD_NO_SAVE\n");
    return 0;
}

int flip_rtc_commit(
    flip_core_adapter_t *core,
    const flip_save_context_t *context)
{
    if (!core || !context || !context->system_id || !context->rom_path)
        return -1;

    if (migrate_legacy_save_files(context) < 0)
        return -1;

    void *memory = flip_core_rtc_memory_data(core);
    size_t memory_size = flip_core_rtc_memory_size(core);

    if (!memory || memory_size == 0) {
        fprintf(stderr, "FLIP_RTC: SAVE_NO_MEMORY\n");
        return 0;
    }

    if (ensure_save_directories(context) < 0) {
        fprintf(stderr, "FLIP_RTC: SAVE_FAILED stage=mkdir\n");
        return -1;
    }

    char current_path[512];
    char previous_path[512];
    char current_tmp_path[512];
    char previous_tmp_path[512];

    if (flip_save_test_build_path(
            context,
            FLIP_RTC_CURRENT_SUFFIX,
            current_path,
            sizeof(current_path)) < 0 ||
        flip_save_test_build_path(
            context,
            FLIP_RTC_PREVIOUS_SUFFIX,
            previous_path,
            sizeof(previous_path)) < 0 ||
        flip_save_test_build_path(
            context,
            ".rtc.tmp",
            current_tmp_path,
            sizeof(current_tmp_path)) < 0 ||
        flip_save_test_build_path(
            context,
            ".rtc.prev.tmp",
            previous_tmp_path,
            sizeof(previous_tmp_path)) < 0) {
        fprintf(stderr, "FLIP_RTC: SAVE_FAILED stage=path\n");
        return -1;
    }

    if (write_synced_file(
            current_tmp_path,
            memory,
            memory_size) < 0) {
        fprintf(stderr, "FLIP_RTC: SAVE_FAILED stage=write\n");
        return -1;
    }

    struct stat st;
    if (stat(current_path, &st) == 0 &&
        S_ISREG(st.st_mode) &&
        st.st_size >= 0 &&
        (size_t)st.st_size == memory_size) {
        if (copy_synced_file(
                current_path,
                previous_tmp_path,
                previous_path,
                memory_size) < 0) {
            unlink(current_tmp_path);
            fprintf(stderr, "FLIP_RTC: SAVE_FAILED stage=previous\n");
            return -1;
        }
    }

    if (rename(current_tmp_path, current_path) < 0) {
        unlink(current_tmp_path);
        fprintf(stderr, "FLIP_RTC: SAVE_FAILED stage=rename\n");
        return -1;
    }

    fprintf(
        stderr,
        "FLIP_RTC: SAVE_OK path=%s size=%zu\n",
        current_path,
        memory_size);
    return 0;
}
