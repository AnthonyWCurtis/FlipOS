#include "flip_state.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int flip_state_test_build_path(
    const flip_state_context_t *context,
    const char *suffix,
    char *path,
    size_t path_size);

typedef struct {
    size_t serialize_size;
    unsigned char accepted[32];
    size_t accepted_size;
    unsigned char serialized[32];
    size_t serialized_size;
    int serialize_result;
} fake_core_state_t;

static int fake_ok(void *userdata)
{
    (void)userdata;
    return 0;
}

static void fake_void(void *userdata)
{
    (void)userdata;
}

static int fake_load(void *userdata, const char *path)
{
    (void)userdata;
    (void)path;
    return 0;
}

static int fake_info(void *userdata, flip_core_info_t *info)
{
    (void)userdata;
    (void)info;
    return 0;
}

static int fake_input(void *userdata, const flip_game_input_t *input)
{
    (void)userdata;
    (void)input;
    return 0;
}

static size_t fake_serialize_size(void *userdata)
{
    fake_core_state_t *state = userdata;
    return state->serialize_size;
}

static int fake_serialize(void *userdata, void *data, size_t size)
{
    fake_core_state_t *state = userdata;

    if (state->serialize_result < 0)
        return -1;

    if (!data || size != state->serialized_size)
        return -1;

    memcpy(data, state->serialized, size);
    return 0;
}

static int fake_unserialize(void *userdata, const void *data, size_t size)
{
    fake_core_state_t *state = userdata;

    if (size != state->accepted_size)
        return -1;

    return memcmp(data, state->accepted, size) == 0 ? 0 : -1;
}

static flip_core_adapter_t make_fake_core(fake_core_state_t *state)
{
    flip_core_adapter_t core = {
        .userdata = state,
        .init = fake_ok,
        .deinit = fake_void,
        .load_game = fake_load,
        .unload_game = fake_void,
        .get_info = fake_info,
        .set_input = fake_input,
        .run_frame = fake_ok,
        .serialize_size = fake_serialize_size,
        .serialize = fake_serialize,
        .unserialize = fake_unserialize,
    };

    return core;
}

static void reset_test_root(void)
{
    char command[768];
    int written = snprintf(
        command,
        sizeof(command),
        "rm -rf '%s'",
        FLIP_STATE_ROOT);

    assert(written > 0 && (size_t)written < sizeof(command));
    assert(system(command) == 0);
    assert(mkdir(FLIP_STATE_ROOT, 0700) == 0);

    char system_dir[640];
    written = snprintf(
        system_dir,
        sizeof(system_dir),
        "%s/gb",
        FLIP_STATE_ROOT);

    assert(written > 0 && (size_t)written < sizeof(system_dir));
    assert(mkdir(system_dir, 0700) == 0);
}

static void write_bytes(
    const flip_state_context_t *context,
    const char *suffix,
    const unsigned char *data,
    size_t size)
{
    char path[640];
    assert(flip_state_test_build_path(
        context,
        suffix,
        path,
        sizeof(path)) == 0);

    FILE *file = fopen(path, "wb");
    assert(file != NULL);
    assert(fwrite(data, 1, size, file) == size);
    assert(fclose(file) == 0);
}

static void read_bytes(
    const flip_state_context_t *context,
    const char *suffix,
    unsigned char *data,
    size_t size)
{
    char path[640];
    assert(flip_state_test_build_path(
        context,
        suffix,
        path,
        sizeof(path)) == 0);

    FILE *file = fopen(path, "rb");
    assert(file != NULL);
    assert(fread(data, 1, size, file) == size);
    assert(fgetc(file) == EOF);
    assert(fclose(file) == 0);
}

static int path_exists(
    const flip_state_context_t *context,
    const char *suffix)
{
    char path[640];
    struct stat st;

    assert(flip_state_test_build_path(
        context,
        suffix,
        path,
        sizeof(path)) == 0);

    return stat(path, &st) == 0;
}

static void test_context_init_preserves_identity(void)
{
    flip_state_context_t context;

    flip_state_context_init(
        &context,
        "gb",
        "/mnt/SDCARD/Roms/GB/Pokemon - Yellow.gb");

    assert(strcmp(context.system_id, "gb") == 0);
    assert(strcmp(
        context.rom_path,
        "/mnt/SDCARD/Roms/GB/Pokemon - Yellow.gb") == 0);
    assert(context.current_was_restored == 0);
}

static void test_legacy_gb_state_path_uses_frozen_fnv1a_identity(void)
{
    flip_state_context_t context;
    char path[512];

    flip_state_context_init(
        &context,
        "gb",
        "/mnt/SDCARD/Roms/GB/Pokemon - Yellow.gb");

    assert(flip_state_test_build_path(
        &context,
        ".state",
        path,
        sizeof(path)) == 0);

    assert(strcmp(
        path,
        FLIP_STATE_ROOT "/gb/792f9628ba43b7d9.state") == 0);
}

static void test_state_sibling_paths_share_legacy_primary_identity(void)
{
    flip_state_context_t context;
    char path[512];

    flip_state_context_init(
        &context,
        "gb",
        "/mnt/SDCARD/Roms/GB/Pokemon - Yellow.gb");

    assert(flip_state_test_build_path(
        &context,
        ".state.tmp",
        path,
        sizeof(path)) == 0);
    assert(strcmp(
        path,
        FLIP_STATE_ROOT "/gb/792f9628ba43b7d9.state.tmp") == 0);

    assert(flip_state_test_build_path(
        &context,
        ".state.prev",
        path,
        sizeof(path)) == 0);
    assert(strcmp(
        path,
        FLIP_STATE_ROOT "/gb/792f9628ba43b7d9.state.prev") == 0);

    assert(flip_state_test_build_path(
        &context,
        ".state.prev.tmp",
        path,
        sizeof(path)) == 0);
    assert(strcmp(
        path,
        FLIP_STATE_ROOT "/gb/792f9628ba43b7d9.state.prev.tmp") == 0);
}

static void init_restore_fixture(
    flip_state_context_t *context,
    fake_core_state_t *state,
    flip_core_adapter_t *core)
{
    static const unsigned char good[] = { 0x10, 0x20, 0x30, 0x40 };
    static const unsigned char next[] = { 0x50, 0x60, 0x70, 0x80 };

    reset_test_root();
    flip_state_context_init(context, "gb", "/roms/test.gb");

    memset(state, 0, sizeof(*state));
    state->serialize_size = sizeof(good);
    state->accepted_size = sizeof(good);
    memcpy(state->accepted, good, sizeof(good));
    state->serialized_size = sizeof(next);
    memcpy(state->serialized, next, sizeof(next));
    state->serialize_result = 0;

    *core = make_fake_core(state);
}

static void test_restore_none_when_no_state_exists(void)
{
    flip_state_context_t context;
    fake_core_state_t state;
    flip_core_adapter_t core;

    init_restore_fixture(&context, &state, &core);

    assert(flip_state_restore(&core, &context) == FLIP_STATE_RESTORE_NONE);
    assert(context.current_was_restored == 0);
}

static void test_restore_valid_current_marks_current_known_good(void)
{
    flip_state_context_t context;
    fake_core_state_t state;
    flip_core_adapter_t core;

    init_restore_fixture(&context, &state, &core);
    write_bytes(&context, ".state", state.accepted, state.accepted_size);

    assert(flip_state_restore(&core, &context) == FLIP_STATE_RESTORE_CURRENT);
    assert(context.current_was_restored == 1);
}

static void test_restore_wrong_size_current_is_unusable(void)
{
    flip_state_context_t context;
    fake_core_state_t state;
    flip_core_adapter_t core;
    static const unsigned char short_state[] = { 0x10, 0x20, 0x30 };

    init_restore_fixture(&context, &state, &core);
    write_bytes(&context, ".state", short_state, sizeof(short_state));

    assert(flip_state_restore(&core, &context) == FLIP_STATE_RESTORE_UNUSABLE);
    assert(context.current_was_restored == 0);
}

static void test_restore_rejected_current_is_unusable(void)
{
    flip_state_context_t context;
    fake_core_state_t state;
    flip_core_adapter_t core;
    static const unsigned char bad[] = { 0xaa, 0xbb, 0xcc, 0xdd };

    init_restore_fixture(&context, &state, &core);
    write_bytes(&context, ".state", bad, sizeof(bad));

    assert(flip_state_restore(&core, &context) == FLIP_STATE_RESTORE_UNUSABLE);
    assert(context.current_was_restored == 0);
}

static void test_restore_falls_back_to_valid_previous(void)
{
    flip_state_context_t context;
    fake_core_state_t state;
    flip_core_adapter_t core;
    static const unsigned char bad[] = { 0xaa, 0xbb, 0xcc, 0xdd };

    init_restore_fixture(&context, &state, &core);
    write_bytes(&context, ".state", bad, sizeof(bad));
    write_bytes(&context, ".state.prev", state.accepted, state.accepted_size);

    assert(flip_state_restore(&core, &context) == FLIP_STATE_RESTORE_PREVIOUS);
    assert(context.current_was_restored == 0);
}

static void test_restore_both_invalid_is_unusable(void)
{
    flip_state_context_t context;
    fake_core_state_t state;
    flip_core_adapter_t core;
    static const unsigned char bad[] = { 0xaa, 0xbb, 0xcc, 0xdd };

    init_restore_fixture(&context, &state, &core);
    write_bytes(&context, ".state", bad, sizeof(bad));
    write_bytes(&context, ".state.prev", bad, sizeof(bad));

    assert(flip_state_restore(&core, &context) == FLIP_STATE_RESTORE_UNUSABLE);
    assert(context.current_was_restored == 0);
}

static void test_restore_io_failure_is_error(void)
{
    flip_state_context_t context;
    fake_core_state_t state;
    flip_core_adapter_t core;
    char path[640];

    init_restore_fixture(&context, &state, &core);
    assert(flip_state_test_build_path(
        &context,
        ".state",
        path,
        sizeof(path)) == 0);
    assert(mkdir(path, 0700) == 0);

    assert(flip_state_restore(&core, &context) == FLIP_STATE_RESTORE_ERROR);
    assert(context.current_was_restored == 0);
}

static void test_restore_ignores_stale_temp_state(void)
{
    flip_state_context_t context;
    fake_core_state_t state;
    flip_core_adapter_t core;

    init_restore_fixture(&context, &state, &core);
    write_bytes(&context, ".state.tmp", state.accepted, state.accepted_size);

    assert(flip_state_restore(&core, &context) == FLIP_STATE_RESTORE_NONE);
    assert(context.current_was_restored == 0);
}

static void test_save_zero_serialize_size_fails(void)
{
    flip_state_context_t context;
    fake_core_state_t state;
    flip_core_adapter_t core;

    init_restore_fixture(&context, &state, &core);
    state.serialize_size = 0;

    assert(flip_state_save(&core, &context) < 0);
    assert(!path_exists(&context, ".state"));
}

static void test_save_serialize_rejection_fails(void)
{
    flip_state_context_t context;
    fake_core_state_t state;
    flip_core_adapter_t core;

    init_restore_fixture(&context, &state, &core);
    state.serialize_result = -1;

    assert(flip_state_save(&core, &context) < 0);
    assert(!path_exists(&context, ".state"));
}

static void test_save_commits_exact_bytes_and_removes_temp(void)
{
    flip_state_context_t context;
    fake_core_state_t state;
    flip_core_adapter_t core;
    unsigned char got[4];

    init_restore_fixture(&context, &state, &core);

    assert(flip_state_save(&core, &context) == 0);
    read_bytes(&context, ".state", got, sizeof(got));
    assert(memcmp(got, state.serialized, sizeof(got)) == 0);
    assert(!path_exists(&context, ".state.tmp"));
    assert(context.current_was_restored == 0);
}

static void test_save_preserves_verified_current_as_previous(void)
{
    flip_state_context_t context;
    fake_core_state_t state;
    flip_core_adapter_t core;
    unsigned char current[4];
    unsigned char previous[4];

    init_restore_fixture(&context, &state, &core);
    write_bytes(&context, ".state", state.accepted, state.accepted_size);

    assert(flip_state_restore(&core, &context) == FLIP_STATE_RESTORE_CURRENT);
    assert(context.current_was_restored == 1);
    assert(flip_state_save(&core, &context) == 0);

    read_bytes(&context, ".state", current, sizeof(current));
    read_bytes(&context, ".state.prev", previous, sizeof(previous));

    assert(memcmp(current, state.serialized, sizeof(current)) == 0);
    assert(memcmp(previous, state.accepted, sizeof(previous)) == 0);
    assert(!path_exists(&context, ".state.prev.tmp"));
    assert(context.current_was_restored == 0);
}

static void test_save_unverified_current_never_replaces_previous(void)
{
    flip_state_context_t context;
    fake_core_state_t state;
    flip_core_adapter_t core;
    static const unsigned char unverified[] = { 0xaa, 0xbb, 0xcc, 0xdd };
    static const unsigned char proven_previous[] = { 0x01, 0x02, 0x03, 0x04 };
    unsigned char previous[4];

    init_restore_fixture(&context, &state, &core);
    write_bytes(&context, ".state", unverified, sizeof(unverified));
    write_bytes(&context, ".state.prev", proven_previous, sizeof(proven_previous));

    context.current_was_restored = 0;
    assert(flip_state_save(&core, &context) == 0);

    read_bytes(&context, ".state.prev", previous, sizeof(previous));
    assert(memcmp(previous, proven_previous, sizeof(previous)) == 0);
}

static void test_save_write_failure_preserves_committed_current(void)
{
    flip_state_context_t context;
    fake_core_state_t state;
    flip_core_adapter_t core;
    unsigned char current[4];
    char tmp_path[640];

    init_restore_fixture(&context, &state, &core);
    write_bytes(&context, ".state", state.accepted, state.accepted_size);

    assert(flip_state_test_build_path(
        &context,
        ".state.tmp",
        tmp_path,
        sizeof(tmp_path)) == 0);
    assert(mkdir(tmp_path, 0700) == 0);

    assert(flip_state_save(&core, &context) < 0);
    read_bytes(&context, ".state", current, sizeof(current));
    assert(memcmp(current, state.accepted, sizeof(current)) == 0);
}

int main(void)
{
    test_context_init_preserves_identity();
    test_legacy_gb_state_path_uses_frozen_fnv1a_identity();
    test_state_sibling_paths_share_legacy_primary_identity();
    test_restore_none_when_no_state_exists();
    test_restore_valid_current_marks_current_known_good();
    test_restore_wrong_size_current_is_unusable();
    test_restore_rejected_current_is_unusable();
    test_restore_falls_back_to_valid_previous();
    test_restore_both_invalid_is_unusable();
    test_restore_io_failure_is_error();
    test_restore_ignores_stale_temp_state();
    test_save_zero_serialize_size_fails();
    test_save_serialize_rejection_fails();
    test_save_commits_exact_bytes_and_removes_temp();
    test_save_preserves_verified_current_as_previous();
    test_save_unverified_current_never_replaces_previous();
    test_save_write_failure_preserves_committed_current();

    puts("flip_state tests passed");
    return 0;
}
