#include "flip_session_persistence.h"
#include "flip_paths.h"

#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void reset_root(void)
{
    char command[1024];

    snprintf(
        command,
        sizeof(command),
        "rm -rf '%s'",
        FLIP_STATE_DATA_ROOT);

    assert(system(command) == 0);

    snprintf(
        command,
        sizeof(command),
        "mkdir -p '%s'",
        FLIP_STORAGE_ROOT);

    assert(system(command) == 0);
}

static flip_session_state_t sample_state(void)
{
    flip_session_state_t state;
    memset(&state, 0, sizeof(state));

    state.valid = 1;
    strcpy(state.state, "suspended");
    strcpy(state.system, "GB");
    strcpy(state.rom, "/mnt/SDCARD/Roms/GB/Test.gb");
    strcpy(state.program, "/mnt/SDCARD/flipos-dev/bin/flip-gb");
    state.pid = 1234;
    state.started = 100;
    state.ended = 200;
    state.exit_code = 20;
    state.signal_number = 0;

    return state;
}

static void test_missing(void)
{
    reset_root();

    flip_session_state_t state;

    assert(flip_session_persistence_read_current(
        &state) == 0);
}

static void test_round_trip_and_exact_format(void)
{
    reset_root();

    flip_session_state_t source =
        sample_state();

    assert(flip_session_persistence_write_current(
        &source) == 0);

    flip_session_state_t loaded;

    assert(flip_session_persistence_read_current(
        &loaded) == 1);

    assert(loaded.valid);
    assert(strcmp(loaded.state, source.state) == 0);
    assert(strcmp(loaded.system, source.system) == 0);
    assert(strcmp(loaded.rom, source.rom) == 0);
    assert(strcmp(loaded.program, source.program) == 0);
    assert(loaded.pid == source.pid);
    assert(loaded.started == source.started);
    assert(loaded.ended == source.ended);
    assert(loaded.exit_code == source.exit_code);
    assert(loaded.signal_number == source.signal_number);

    FILE *f =
        fopen(
            FLIP_SESSION_FILE,
            "r");

    assert(f != NULL);

    char content[2048];
    size_t count =
        fread(
            content,
            1,
            sizeof(content) - 1,
            f);

    content[count] = '\0';
    fclose(f);

    assert(strcmp(
        content,
        "version=1\n"
        "state=suspended\n"
        "system=GB\n"
        "rom=/mnt/SDCARD/Roms/GB/Test.gb\n"
        "program=/mnt/SDCARD/flipos-dev/bin/flip-gb\n"
        "pid=1234\n"
        "started=100\n"
        "ended=200\n"
        "exit_code=20\n"
        "signal=0\n") == 0);
}

static void test_malformed_and_quarantine(void)
{
    reset_root();

    assert(system(
        "mkdir -p " FLIP_SESSION_ROOT) == 0);

    FILE *f =
        fopen(
            FLIP_SESSION_FILE,
            "w");

    assert(f != NULL);
    assert(fputs(
        "version=1\n"
        "state=running\n"
        "system=GB\n",
        f) >= 0);
    fclose(f);

    flip_session_state_t state;

    assert(flip_session_persistence_read_current(
        &state) < 0);

    char quarantine[1024];

    assert(flip_session_persistence_quarantine_current(
        quarantine,
        sizeof(quarantine)) == 1);

    assert(access(
        FLIP_SESSION_FILE,
        F_OK) != 0);

    assert(strstr(
        quarantine,
        ".invalid.") != NULL);

    assert(access(
        quarantine,
        F_OK) == 0);
}

static void test_invalid_write_is_non_destructive(void)
{
    reset_root();

    flip_session_state_t good =
        sample_state();

    assert(flip_session_persistence_write_current(
        &good) == 0);

    flip_session_state_t bad =
        good;

    strcpy(
        bad.state,
        "bad\nstate");

    assert(flip_session_persistence_write_current(
        &bad) < 0);

    flip_session_state_t loaded;

    assert(flip_session_persistence_read_current(
        &loaded) == 1);

    assert(strcmp(
        loaded.state,
        "suspended") == 0);
}

int main(void)
{
    assert(flip_session_persistence_read_current(NULL) < 0);
    assert(flip_session_persistence_write_current(NULL) < 0);

    test_missing();
    test_round_trip_and_exact_format();
    test_malformed_and_quarantine();
    test_invalid_write_is_non_destructive();

    printf("flip_session_persistence tests passed\n");
    return EXIT_SUCCESS;
}
