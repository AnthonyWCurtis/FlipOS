#include "flip_state.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int flip_state_test_build_path(
    const flip_state_context_t *context,
    const char *suffix,
    char *path,
    size_t path_size);

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
        FLIP_STATE_ROOT "/gb/cd6eacde6a72323b.state") == 0);
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
        FLIP_STATE_ROOT "/gb/cd6eacde6a72323b.state.tmp") == 0);

    assert(flip_state_test_build_path(
        &context,
        ".state.prev",
        path,
        sizeof(path)) == 0);
    assert(strcmp(
        path,
        FLIP_STATE_ROOT "/gb/cd6eacde6a72323b.state.prev") == 0);

    assert(flip_state_test_build_path(
        &context,
        ".state.prev.tmp",
        path,
        sizeof(path)) == 0);
    assert(strcmp(
        path,
        FLIP_STATE_ROOT "/gb/cd6eacde6a72323b.state.prev.tmp") == 0);
}

int main(void)
{
    test_context_init_preserves_identity();
    test_legacy_gb_state_path_uses_frozen_fnv1a_identity();
    test_state_sibling_paths_share_legacy_primary_identity();

    puts("flip_state path tests passed");
    return 0;
}
