#include "flip_game_data.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef FLIP_GAME_DATA_TEST_ROOT
#define FLIP_GAME_DATA_TEST_ROOT "/tmp/flipos-game-data-reset-test"
#endif

static void mkdir_or_die(const char *path)
{
    if (mkdir(path, 0700) < 0 && errno != EEXIST) {
        perror(path);
        exit(1);
    }
}

static void write_file(
    const char *path,
    const char *contents)
{
    FILE *f = fopen(path, "wb");
    assert(f);

    size_t size = strlen(contents);
    assert(fwrite(contents, 1, size, f) == size);
    assert(fclose(f) == 0);
}

static int exists(const char *path)
{
    return access(path, F_OK) == 0;
}

static void reset_root(void)
{
    char command[1024];

    int written = snprintf(
        command,
        sizeof(command),
        "rm -rf '%s'",
        FLIP_GAME_DATA_TEST_ROOT);

    assert(written > 0);
    assert((size_t)written < sizeof(command));
    assert(system(command) == 0);

    mkdir_or_die(FLIP_GAME_DATA_TEST_ROOT);
}

static void make_dirs(void)
{
    char path[1024];

    const char *dirs[] = {
        "saves",
        "saves/gb",
        "states",
        "states/gb",
        "sessions",
        "sessions/gb",
        "reset",
    };

    for (size_t i = 0;
         i < sizeof(dirs) / sizeof(dirs[0]);
         ++i) {

        int written = snprintf(
            path,
            sizeof(path),
            "%s/%s",
            FLIP_GAME_DATA_TEST_ROOT,
            dirs[i]);

        assert(written > 0);
        assert((size_t)written < sizeof(path));

        mkdir_or_die(path);
    }
}

static void test_reset_quarantines_only_target_game(void)
{
    reset_root();
    make_dirs();

    const char *target =
        "/mnt/SDCARD/Roms/GB/Pokemon - Yellow.gb";

    const char *other =
        "/mnt/SDCARD/Roms/GB/Other.gb";

    /*
     * Yellow's frozen FNV identity is already established in
     * existing save/state/session tests.
     */
    const char *target_id =
        "792f9628ba43b7d9";

    /*
     * The other game's exact identity does not matter here,
     * only that its files remain untouched.
     */
    const char *other_id =
        "1111111111111111";

    const char *target_suffixes[] = {
        "saves/gb/792f9628ba43b7d9.sav",
        "saves/gb/792f9628ba43b7d9.sav.prev",
        "saves/gb/792f9628ba43b7d9.sav.recovery",
        "states/gb/792f9628ba43b7d9.state",
        "states/gb/792f9628ba43b7d9.state.prev",
        "states/gb/792f9628ba43b7d9.state.recovery",
        "sessions/gb/792f9628ba43b7d9.restore-pending",
        "sessions/gb/792f9628ba43b7d9.session",
    };

    const char *other_suffixes[] = {
        "saves/gb/1111111111111111.sav",
        "states/gb/1111111111111111.state",
        "sessions/gb/1111111111111111.session",
    };

    char path[1024];

    for (size_t i = 0;
         i < sizeof(target_suffixes) / sizeof(target_suffixes[0]);
         ++i) {

        snprintf(
            path,
            sizeof(path),
            "%s/%s",
            FLIP_GAME_DATA_TEST_ROOT,
            target_suffixes[i]);

        write_file(path, "TARGET");
    }

    for (size_t i = 0;
         i < sizeof(other_suffixes) / sizeof(other_suffixes[0]);
         ++i) {

        snprintf(
            path,
            sizeof(path),
            "%s/%s",
            FLIP_GAME_DATA_TEST_ROOT,
            other_suffixes[i]);

        write_file(path, "OTHER");
    }

    assert(flip_game_data_reset(
        "gb",
        "gb",
        target) == 0);

    for (size_t i = 0;
         i < sizeof(target_suffixes) / sizeof(target_suffixes[0]);
         ++i) {

        snprintf(
            path,
            sizeof(path),
            "%s/%s",
            FLIP_GAME_DATA_TEST_ROOT,
            target_suffixes[i]);

        assert(!exists(path));
    }

    /*
     * Reset must be recoverable: every target artifact must exist
     * in the quarantine/reset area after leaving its active path.
     */
    for (size_t i = 0;
         i < sizeof(target_suffixes) / sizeof(target_suffixes[0]);
         ++i) {

        const char *base =
            strrchr(target_suffixes[i], '/');

        assert(base);
        ++base;

        snprintf(
            path,
            sizeof(path),
            "%s/reset/%s",
            FLIP_GAME_DATA_TEST_ROOT,
            base);

        assert(exists(path));
    }

    for (size_t i = 0;
         i < sizeof(other_suffixes) / sizeof(other_suffixes[0]);
         ++i) {

        snprintf(
            path,
            sizeof(path),
            "%s/%s",
            FLIP_GAME_DATA_TEST_ROOT,
            other_suffixes[i]);

        assert(exists(path));
    }

    (void)other;
    (void)target_id;
    (void)other_id;
}


static void test_reset_with_no_files_succeeds(void)
{
    reset_root();
    make_dirs();

    assert(flip_game_data_reset(
        "gb",
        "gb",
        "/mnt/SDCARD/Roms/GB/Empty.gb") == 0);
}

static void test_invalid_inputs_fail_closed(void)
{
    reset_root();
    make_dirs();

    assert(flip_game_data_reset(
        NULL,
        "gb",
        "/mnt/SDCARD/Roms/GB/Test.gb") == -1);

    assert(flip_game_data_reset(
        "",
        "gb",
        "/mnt/SDCARD/Roms/GB/Test.gb") == -1);

    assert(flip_game_data_reset(
        "gb/../../bad",
        "gb",
        "/mnt/SDCARD/Roms/GB/Test.gb") == -1);

    assert(flip_game_data_reset(
        "gb",
        "gb",
        NULL) == -1);

    assert(flip_game_data_reset(
        "gb",
        "gb",
        "") == -1);
}

static void test_existing_quarantine_refuses_overwrite(void)
{
    reset_root();
    make_dirs();

    const char *rom =
        "/mnt/SDCARD/Roms/GB/Pokemon - Yellow.gb";

    char active[1024];
    char quarantine[1024];

    snprintf(
        active,
        sizeof(active),
        "%s/saves/gb/792f9628ba43b7d9.sav",
        FLIP_GAME_DATA_TEST_ROOT);

    snprintf(
        quarantine,
        sizeof(quarantine),
        "%s/reset/792f9628ba43b7d9.sav",
        FLIP_GAME_DATA_TEST_ROOT);

    write_file(active, "ACTIVE");
    write_file(quarantine, "OLDER RESET");

    assert(flip_game_data_reset(
        "gb",
        "gb",
        rom) == -1);

    assert(exists(active));
    assert(exists(quarantine));
}

static void test_invalid_quarantine_destination_fails_before_moves(void)
{
    reset_root();
    make_dirs();

    const char *rom =
        "/mnt/SDCARD/Roms/GB/Pokemon - Yellow.gb";

    char save_path[1024];
    char state_path[1024];
    char blocked_destination[1024];

    snprintf(
        save_path,
        sizeof(save_path),
        "%s/saves/gb/792f9628ba43b7d9.sav",
        FLIP_GAME_DATA_TEST_ROOT);

    snprintf(
        state_path,
        sizeof(state_path),
        "%s/states/gb/792f9628ba43b7d9.state",
        FLIP_GAME_DATA_TEST_ROOT);

    snprintf(
        blocked_destination,
        sizeof(blocked_destination),
        "%s/reset/792f9628ba43b7d9.state",
        FLIP_GAME_DATA_TEST_ROOT);

    write_file(save_path, "SAVE");
    write_file(state_path, "STATE");

    /*
     * Make the state quarantine destination non-regular.
     * The reset must fail closed without losing the save.
     */
    assert(mkdir(
        blocked_destination,
        0700) == 0);

    assert(flip_game_data_reset(
        "gb",
        "gb",
        rom) == -1);

    assert(exists(save_path));
    assert(exists(state_path));
}


static void test_gbc_uses_shared_gb_persistence_namespace(void)
{
    reset_root();
    make_dirs();

    const char *rom =
        "/mnt/SDCARD/Roms/GBC/Pokemon - Crystal.gbc";

    char save[1024];
    char state[1024];
    char session_dir[1024];
    char session[1024];

    snprintf(
        save,
        sizeof(save),
        "%s/saves/gb/259f8b87baf5b937.sav",
        FLIP_GAME_DATA_TEST_ROOT);

    snprintf(
        state,
        sizeof(state),
        "%s/states/gb/259f8b87baf5b937.state",
        FLIP_GAME_DATA_TEST_ROOT);

    snprintf(
        session_dir,
        sizeof(session_dir),
        "%s/sessions/GBC",
        FLIP_GAME_DATA_TEST_ROOT);

    mkdir_or_die(session_dir);

    snprintf(
        session,
        sizeof(session),
        "%s/sessions/GBC/259f8b87baf5b937.session",
        FLIP_GAME_DATA_TEST_ROOT);

    write_file(save, "GBC SAVE");
    write_file(state, "GBC STATE");
    write_file(session, "GBC SESSION");

    /*
     * Public system identity is GBC, while persistence is owned
     * by the shared GB/GBC adapter namespace "gb".
     */
    assert(flip_game_data_reset(
        "GBC",
        "gb",
        rom) == 0);

    assert(!exists(save));
    assert(!exists(state));
    assert(!exists(session));

    char reset_save[1024];
    char reset_state[1024];
    char reset_session[1024];

    snprintf(
        reset_save,
        sizeof(reset_save),
        "%s/reset/259f8b87baf5b937.sav",
        FLIP_GAME_DATA_TEST_ROOT);

    snprintf(
        reset_state,
        sizeof(reset_state),
        "%s/reset/259f8b87baf5b937.state",
        FLIP_GAME_DATA_TEST_ROOT);

    snprintf(
        reset_session,
        sizeof(reset_session),
        "%s/reset/259f8b87baf5b937.session",
        FLIP_GAME_DATA_TEST_ROOT);

    assert(exists(reset_save));
    assert(exists(reset_state));
    assert(exists(reset_session));
}

int main(void)
{
    test_reset_quarantines_only_target_game();
    test_gbc_uses_shared_gb_persistence_namespace();
    test_reset_with_no_files_succeeds();
    test_invalid_inputs_fail_closed();
    test_existing_quarantine_refuses_overwrite();
    test_invalid_quarantine_destination_fails_before_moves();

    puts("game-data reset tests passed");
    return 0;
}
