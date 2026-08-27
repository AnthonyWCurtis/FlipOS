#include "flip_game_data.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef FLIP_GAME_DATA_TEST_ROOT
#define FLIP_GAME_DATA_TEST_ROOT "/tmp/flipos-game-data-reset-rollback-test"
#endif

static void mkdir_or_die(const char *path)
{
    if (mkdir(path, 0700) < 0 &&
        errno != EEXIST) {
        perror(path);
        exit(1);
    }
}

static int exists(const char *path)
{
    return access(path, F_OK) == 0;
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

static void setup(void)
{
    char command[1024];

    snprintf(
        command,
        sizeof(command),
        "rm -rf '%s'",
        FLIP_GAME_DATA_TEST_ROOT);

    assert(system(command) == 0);

    mkdir_or_die(FLIP_GAME_DATA_TEST_ROOT);

    const char *dirs[] = {
        "saves",
        "saves/gb",
        "states",
        "states/gb",
        "sessions",
        "sessions/gb",
        "reset",
    };

    char path[1024];

    for (size_t i = 0;
         i < sizeof(dirs) / sizeof(dirs[0]);
         ++i) {

        snprintf(
            path,
            sizeof(path),
            "%s/%s",
            FLIP_GAME_DATA_TEST_ROOT,
            dirs[i]);

        mkdir_or_die(path);
    }
}

int main(void)
{
    setup();

    const char *rom =
        "/mnt/SDCARD/Roms/GB/Pokemon - Yellow.gb";

    char save[1024];
    char state[1024];
    char quarantined_save[1024];

    snprintf(
        save,
        sizeof(save),
        "%s/saves/gb/792f9628ba43b7d9.sav",
        FLIP_GAME_DATA_TEST_ROOT);

    snprintf(
        state,
        sizeof(state),
        "%s/states/gb/792f9628ba43b7d9.state",
        FLIP_GAME_DATA_TEST_ROOT);

    snprintf(
        quarantined_save,
        sizeof(quarantined_save),
        "%s/reset/792f9628ba43b7d9.sav",
        FLIP_GAME_DATA_TEST_ROOT);

    write_file(save, "SAVE");
    write_file(state, "STATE");

    /*
     * Runtime is compiled to fail after the first successful move.
     * The save must be rolled back to its active path.
     */
    assert(flip_game_data_reset(
        "gb",
        rom) == -1);

    assert(exists(save));
    assert(exists(state));
    assert(!exists(quarantined_save));

    puts("game-data reset rollback test passed");
    return 0;
}
