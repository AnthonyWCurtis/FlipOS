#include "flip_recovery.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int flip_save_test_build_path(
    const flip_save_context_t *context,
    const char *suffix,
    char *path,
    size_t path_size);

int flip_state_test_build_path(
    const flip_state_context_t *context,
    const char *suffix,
    char *path,
    size_t path_size);

int flip_recovery_test_build_path(
    const flip_recovery_context_t *context,
    const char *suffix,
    char *path,
    size_t path_size);

typedef struct {
    unsigned char save_memory[32];
    size_t save_memory_size;
} fake_core_t;

static void *fake_save_memory_data(void *userdata)
{
    fake_core_t *fake = userdata;
    return fake->save_memory;
}

static size_t fake_save_memory_size(void *userdata)
{
    fake_core_t *fake = userdata;
    return fake->save_memory_size;
}

static flip_core_adapter_t make_core(fake_core_t *fake)
{
    flip_core_adapter_t core;
    memset(&core, 0, sizeof(core));
    core.userdata = fake;
    core.save_memory_data = fake_save_memory_data;
    core.save_memory_size = fake_save_memory_size;
    return core;
}

static void mkdir_ok(const char *path)
{
    if (mkdir(path, 0700) < 0)
        assert(access(path, F_OK) == 0);
}

static void remove_save_file(const flip_save_context_t *context, const char *suffix)
{
    char path[640];
    if (flip_save_test_build_path(context, suffix, path, sizeof(path)) == 0) {
        unlink(path);
        rmdir(path);
    }
}

static void remove_state_file(const flip_state_context_t *context, const char *suffix)
{
    char path[640];
    if (flip_state_test_build_path(context, suffix, path, sizeof(path)) == 0) {
        unlink(path);
        rmdir(path);
    }
}

static void reset_roots(
    const flip_save_context_t *save,
    const flip_state_context_t *state)
{
    static const char *const save_suffixes[] = {
        ".sav", ".sav.prev", ".sav.tmp", ".sav.prev.tmp",
        ".sav.recovery", ".sav.recovery.tmp"
    };
    static const char *const state_suffixes[] = {
        ".state", ".state.prev", ".state.tmp", ".state.prev.tmp",
        ".state.recovery", ".state.recovery.tmp"
    };

    for (size_t i = 0; i < sizeof(save_suffixes) / sizeof(save_suffixes[0]); ++i)
        remove_save_file(save, save_suffixes[i]);
    for (size_t i = 0; i < sizeof(state_suffixes) / sizeof(state_suffixes[0]); ++i)
        remove_state_file(state, state_suffixes[i]);

    char path[640];
    snprintf(path, sizeof(path), "%s/gb", FLIP_SAVE_ROOT);
    rmdir(path);
    rmdir(FLIP_SAVE_ROOT);
    snprintf(path, sizeof(path), "%s/gb", FLIP_STATE_ROOT);
    rmdir(path);
    rmdir(FLIP_STATE_ROOT);
    snprintf(path, sizeof(path), "%s/gb", FLIP_SESSION_RECOVERY_ROOT);
    rmdir(path);
    rmdir(FLIP_SESSION_RECOVERY_ROOT);

    mkdir_ok(FLIP_SAVE_ROOT);
    mkdir_ok(FLIP_STATE_ROOT);
    mkdir_ok(FLIP_SESSION_RECOVERY_ROOT);
    snprintf(path, sizeof(path), "%s/gb", FLIP_SAVE_ROOT);
    mkdir_ok(path);
    snprintf(path, sizeof(path), "%s/gb", FLIP_STATE_ROOT);
    mkdir_ok(path);
    snprintf(path, sizeof(path), "%s/gb", FLIP_SESSION_RECOVERY_ROOT);
    mkdir_ok(path);
}

static void write_file(const char *path, const unsigned char *data, size_t size)
{
    FILE *file = fopen(path, "wb");
    assert(file != NULL);
    assert(fwrite(data, 1, size, file) == size);
    assert(fclose(file) == 0);
}

static void read_file(const char *path, unsigned char *data, size_t size)
{
    FILE *file = fopen(path, "rb");
    assert(file != NULL);
    assert(fread(data, 1, size, file) == size);
    assert(fgetc(file) == EOF);
    assert(fclose(file) == 0);
}

static void save_path(
    const flip_save_context_t *context,
    const char *suffix,
    char *path,
    size_t path_size)
{
    assert(flip_save_test_build_path(context, suffix, path, path_size) == 0);
}

static void state_path(
    const flip_state_context_t *context,
    const char *suffix,
    char *path,
    size_t path_size)
{
    assert(flip_state_test_build_path(context, suffix, path, path_size) == 0);
}

static int state_exists(const flip_state_context_t *context, const char *suffix)
{
    char path[640];
    struct stat st;
    state_path(context, suffix, path, sizeof(path));
    return stat(path, &st) == 0;
}

static void init_contexts(
    flip_save_context_t *save,
    flip_state_context_t *state,
    flip_recovery_context_t *recovery)
{
    const char *rom = "/mnt/SDCARD/Roms/GB/Pokemon - Yellow.gb";
    flip_save_context_init(save, "gb", rom);
    flip_state_context_init(state, "gb", rom);
    flip_recovery_context_init(recovery, "gb", rom);
}

static void test_restore_previous_marker_failure_does_not_change_active_save(void)
{
    flip_save_context_t save;
    flip_state_context_t state;
    flip_recovery_context_t recovery;
    init_contexts(&save, &state, &recovery);
    reset_roots(&save, &state);

    fake_core_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.save_memory_size = sizeof(fake.save_memory);
    flip_core_adapter_t core = make_core(&fake);

    unsigned char current[32];
    unsigned char previous[32];
    const unsigned char current_state[] = { 1, 3, 5, 7 };
    memset(current, 0x31, sizeof(current));
    memset(previous, 0x72, sizeof(previous));

    char path[640];
    save_path(&save, ".sav", path, sizeof(path));
    write_file(path, current, sizeof(current));
    save_path(&save, ".sav.prev", path, sizeof(path));
    write_file(path, previous, sizeof(previous));
    state_path(&state, ".state", path, sizeof(path));
    write_file(path, current_state, sizeof(current_state));

    /*
     * Force resume-marker creation to fail without changing save storage.
     * A directory at the temporary marker path makes fopen("wb") fail even
     * when the test process has elevated permissions.
     */
    char marker_tmp[640];
    assert(flip_recovery_test_build_path(
        &recovery,
        ".restore-pending.tmp",
        marker_tmp,
        sizeof(marker_tmp)) == 0);
    assert(mkdir(marker_tmp, 0700) == 0);

    assert(flip_recovery_restore_previous(
        &core, &save, &state, &recovery) == FLIP_RECOVERY_ERROR);

    unsigned char got[32];
    save_path(&save, ".sav", path, sizeof(path));
    read_file(path, got, sizeof(got));
    assert(memcmp(got, current, sizeof(got)) == 0);

    save_path(&save, ".sav.recovery", path, sizeof(path));
    assert(access(path, F_OK) != 0);
    assert(flip_recovery_resume_blocked(&recovery) == 0);
    assert(state_exists(&state, ".state"));

    assert(rmdir(marker_tmp) == 0);
}

static void test_restore_previous_preserves_paired_timeline_and_blocks_resume(void)
{
    flip_save_context_t save;
    flip_state_context_t state;
    flip_recovery_context_t recovery;
    init_contexts(&save, &state, &recovery);
    reset_roots(&save, &state);

    fake_core_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.save_memory_size = sizeof(fake.save_memory);
    flip_core_adapter_t core = make_core(&fake);

    unsigned char current[32];
    unsigned char previous[32];
    const unsigned char current_state[] = { 1, 2, 3, 4 };
    for (size_t i = 0; i < sizeof(current); ++i) {
        current[i] = (unsigned char)(0x20 + i);
        previous[i] = (unsigned char)(0x80 + i);
    }

    char path[640];
    save_path(&save, ".sav", path, sizeof(path));
    write_file(path, current, sizeof(current));
    save_path(&save, ".sav.prev", path, sizeof(path));
    write_file(path, previous, sizeof(previous));
    state_path(&state, ".state", path, sizeof(path));
    write_file(path, current_state, sizeof(current_state));

    assert(flip_recovery_restore_previous(
        &core, &save, &state, &recovery) == FLIP_RECOVERY_OK);

    unsigned char got_save[32];
    unsigned char got_recovery[32];
    unsigned char got_state[sizeof(current_state)];
    save_path(&save, ".sav", path, sizeof(path));
    read_file(path, got_save, sizeof(got_save));
    assert(memcmp(got_save, previous, sizeof(got_save)) == 0);
    save_path(&save, ".sav.recovery", path, sizeof(path));
    read_file(path, got_recovery, sizeof(got_recovery));
    assert(memcmp(got_recovery, current, sizeof(got_recovery)) == 0);
    state_path(&state, ".state.recovery", path, sizeof(path));
    read_file(path, got_state, sizeof(got_state));
    assert(memcmp(got_state, current_state, sizeof(got_state)) == 0);
    assert(!state_exists(&state, ".state"));
    assert(flip_recovery_resume_blocked(&recovery) == 1);
}

static void test_restore_previous_without_current_state_clears_stale_recovery_state(void)
{
    flip_save_context_t save;
    flip_state_context_t state;
    flip_recovery_context_t recovery;
    init_contexts(&save, &state, &recovery);
    reset_roots(&save, &state);

    fake_core_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.save_memory_size = sizeof(fake.save_memory);
    flip_core_adapter_t core = make_core(&fake);

    unsigned char current[32];
    unsigned char previous[32];
    const unsigned char stale_state[] = { 9, 9, 9, 9 };
    memset(current, 0x44, sizeof(current));
    memset(previous, 0x55, sizeof(previous));

    char path[640];
    save_path(&save, ".sav", path, sizeof(path));
    write_file(path, current, sizeof(current));
    save_path(&save, ".sav.prev", path, sizeof(path));
    write_file(path, previous, sizeof(previous));
    state_path(&state, ".state.recovery", path, sizeof(path));
    write_file(path, stale_state, sizeof(stale_state));

    assert(flip_recovery_restore_previous(
        &core, &save, &state, &recovery) == FLIP_RECOVERY_OK);
    assert(!state_exists(&state, ".state.recovery"));
    assert(flip_recovery_resume_blocked(&recovery) == 1);
}

static void test_undo_restores_paired_recovery_and_clears_marker(void)
{
    flip_save_context_t save;
    flip_state_context_t state;
    flip_recovery_context_t recovery;
    init_contexts(&save, &state, &recovery);
    reset_roots(&save, &state);

    fake_core_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.save_memory_size = sizeof(fake.save_memory);
    flip_core_adapter_t core = make_core(&fake);

    unsigned char current[32];
    unsigned char recovery_save[32];
    const unsigned char recovery_state[] = { 7, 6, 5, 4 };
    memset(current, 0x33, sizeof(current));
    memset(recovery_save, 0x77, sizeof(recovery_save));

    char path[640];
    save_path(&save, ".sav", path, sizeof(path));
    write_file(path, current, sizeof(current));
    save_path(&save, ".sav.recovery", path, sizeof(path));
    write_file(path, recovery_save, sizeof(recovery_save));
    state_path(&state, ".state.recovery", path, sizeof(path));
    write_file(path, recovery_state, sizeof(recovery_state));
    assert(flip_recovery_block_resume(&recovery) == 0);

    assert(flip_recovery_undo(
        &core, &save, &state, &recovery) == FLIP_RECOVERY_OK);

    unsigned char got_save[32];
    unsigned char got_state[sizeof(recovery_state)];
    save_path(&save, ".sav", path, sizeof(path));
    read_file(path, got_save, sizeof(got_save));
    assert(memcmp(got_save, recovery_save, sizeof(got_save)) == 0);
    state_path(&state, ".state", path, sizeof(path));
    read_file(path, got_state, sizeof(got_state));
    assert(memcmp(got_state, recovery_state, sizeof(got_state)) == 0);
    assert(flip_recovery_resume_blocked(&recovery) == 0);
}

static void test_save_only_undo_keeps_resume_blocked_and_quarantines_current_state(void)
{
    flip_save_context_t save;
    flip_state_context_t state;
    flip_recovery_context_t recovery;
    init_contexts(&save, &state, &recovery);
    reset_roots(&save, &state);

    fake_core_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.save_memory_size = sizeof(fake.save_memory);
    flip_core_adapter_t core = make_core(&fake);

    unsigned char current[32];
    unsigned char recovery_save[32];
    const unsigned char mismatched_state[] = { 0xaa, 0xbb, 0xcc, 0xdd };
    memset(current, 0x11, sizeof(current));
    memset(recovery_save, 0x22, sizeof(recovery_save));

    char path[640];
    save_path(&save, ".sav", path, sizeof(path));
    write_file(path, current, sizeof(current));
    save_path(&save, ".sav.recovery", path, sizeof(path));
    write_file(path, recovery_save, sizeof(recovery_save));
    state_path(&state, ".state", path, sizeof(path));
    write_file(path, mismatched_state, sizeof(mismatched_state));
    assert(flip_recovery_block_resume(&recovery) == 0);

    assert(flip_recovery_undo(
        &core, &save, &state, &recovery) == FLIP_RECOVERY_OK);

    assert(flip_recovery_resume_blocked(&recovery) == 1);
    assert(!state_exists(&state, ".state"));
}

int main(void)
{
    test_restore_previous_marker_failure_does_not_change_active_save();
    test_restore_previous_preserves_paired_timeline_and_blocks_resume();
    test_restore_previous_without_current_state_clears_stale_recovery_state();
    test_undo_restores_paired_recovery_and_clears_marker();
    test_save_only_undo_keeps_resume_blocked_and_quarantines_current_state();

    puts("flip_recovery transaction tests passed");
    return 0;
}
