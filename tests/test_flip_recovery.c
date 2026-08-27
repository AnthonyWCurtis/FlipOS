#include "flip_recovery.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int flip_recovery_test_build_path(
    const flip_recovery_context_t *context,
    const char *suffix,
    char *path,
    size_t path_size);

static void reset_root(void)
{
    char command[768];
    int written = snprintf(
        command,
        sizeof(command),
        "rm -rf '%s'",
        FLIP_SESSION_RECOVERY_ROOT);

    assert(written > 0 && (size_t)written < sizeof(command));
    assert(system(command) == 0);
}

static int path_exists(
    const flip_recovery_context_t *context,
    const char *suffix)
{
    char path[640];
    struct stat st;

    assert(flip_recovery_test_build_path(
        context,
        suffix,
        path,
        sizeof(path)) == 0);

    return stat(path, &st) == 0;
}

static void test_marker_path_uses_frozen_rom_identity(void)
{
    flip_recovery_context_t context;
    char path[640];

    flip_recovery_context_init(
        &context,
        "gb",
        "/mnt/SDCARD/Roms/GB/Pokemon - Yellow.gb");

    assert(flip_recovery_test_build_path(
        &context,
        ".restore-pending",
        path,
        sizeof(path)) == 0);

    assert(strcmp(
        path,
        FLIP_SESSION_RECOVERY_ROOT "/gb/792f9628ba43b7d9.restore-pending") == 0);
}

static void test_absent_marker_allows_resume(void)
{
    reset_root();

    flip_recovery_context_t context;
    flip_recovery_context_init(&context, "gb", "/roms/test.gb");

    assert(flip_recovery_resume_blocked(&context) == 0);
}

static void test_block_resume_creates_persistent_regular_marker(void)
{
    reset_root();

    flip_recovery_context_t context;
    flip_recovery_context_init(&context, "gb", "/roms/test.gb");

    assert(flip_recovery_block_resume(&context) == 0);
    assert(flip_recovery_resume_blocked(&context) == 1);
    assert(path_exists(&context, ".restore-pending"));
    assert(!path_exists(&context, ".restore-pending.tmp"));

    char path[640];
    struct stat st;
    assert(flip_recovery_test_build_path(
        &context,
        ".restore-pending",
        path,
        sizeof(path)) == 0);
    assert(stat(path, &st) == 0);
    assert(S_ISREG(st.st_mode));
    assert(st.st_size == 0);
}

static void test_non_regular_marker_blocks_conservatively(void)
{
    reset_root();

    flip_recovery_context_t context;
    flip_recovery_context_init(&context, "gb", "/roms/test.gb");

    char root[640];
    char system_dir[640];
    char marker[640];

    assert(mkdir(FLIP_SESSION_RECOVERY_ROOT, 0700) == 0);
    assert(snprintf(system_dir, sizeof(system_dir), "%s/gb", FLIP_SESSION_RECOVERY_ROOT) > 0);
    assert(mkdir(system_dir, 0700) == 0);
    (void)root;

    assert(flip_recovery_test_build_path(
        &context,
        ".restore-pending",
        marker,
        sizeof(marker)) == 0);
    assert(mkdir(marker, 0700) == 0);

    assert(flip_recovery_resume_blocked(&context) == 1);
}

static void test_clear_marker_reenables_resume(void)
{
    reset_root();

    flip_recovery_context_t context;
    flip_recovery_context_init(&context, "gb", "/roms/test.gb");

    assert(flip_recovery_block_resume(&context) == 0);
    assert(flip_recovery_clear_resume_block(&context) == 0);
    assert(flip_recovery_resume_blocked(&context) == 0);
    assert(!path_exists(&context, ".restore-pending"));
}

static void test_clear_failure_keeps_resume_blocked(void)
{
    reset_root();

    flip_recovery_context_t context;
    flip_recovery_context_init(&context, "gb", "/roms/test.gb");

    assert(mkdir(FLIP_SESSION_RECOVERY_ROOT, 0700) == 0);

    char system_dir[640];
    assert(snprintf(system_dir, sizeof(system_dir), "%s/gb", FLIP_SESSION_RECOVERY_ROOT) > 0);
    assert(mkdir(system_dir, 0700) == 0);

    char marker[640];
    assert(flip_recovery_test_build_path(
        &context,
        ".restore-pending",
        marker,
        sizeof(marker)) == 0);
    assert(mkdir(marker, 0700) == 0);

    assert(flip_recovery_clear_resume_block(&context) == -1);
    assert(flip_recovery_resume_blocked(&context) == 1);
}

int main(void)
{
    test_marker_path_uses_frozen_rom_identity();
    test_absent_marker_allows_resume();
    test_block_resume_creates_persistent_regular_marker();
    test_non_regular_marker_blocks_conservatively();
    test_clear_marker_reenables_resume();
    test_clear_failure_keeps_resume_blocked();

    puts("flip_recovery tests passed");
    return 0;
}
