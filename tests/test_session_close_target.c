#include "flip_session.h"
#include "flip_system.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef FLIP_SESSION_STATE_ROOT
#define FLIP_SESSION_STATE_ROOT \
    "/tmp/flipos-session-close-test"
#endif

static void reset_root(void)
{
    char command[768];

    int written = snprintf(
        command,
        sizeof(command),
        "rm -rf '%s'",
        FLIP_SESSION_STATE_ROOT);

    assert(written > 0);
    assert((size_t)written < sizeof(command));
    assert(system(command) == 0);
}

static void make_session_dir(void)
{
    char session_dir[1024];

    assert(mkdir(
        FLIP_SESSION_STATE_ROOT,
        0777) == 0);

    int written = snprintf(
        session_dir,
        sizeof(session_dir),
        "%s/session",
        FLIP_SESSION_STATE_ROOT);

    assert(written > 0);
    assert((size_t)written < sizeof(session_dir));

    assert(mkdir(
        session_dir,
        0777) == 0);
}

static void write_current(
    const char *state,
    const char *system,
    const char *rom)
{
    char path[1024];

    int written = snprintf(
        path,
        sizeof(path),
        "%s/session/current",
        FLIP_SESSION_STATE_ROOT);

    assert(written > 0);
    assert((size_t)written < sizeof(path));

    FILE *f = fopen(
        path,
        "w");

    assert(f != NULL);

    assert(fprintf(
        f,
        "version=1\n"
        "state=%s\n"
        "system=%s\n"
        "rom=%s\n"
        "program=/tmp/flip-gb\n"
        "pid=1234\n"
        "started=100\n"
        "ended=200\n"
        "exit_code=20\n"
        "signal=0\n",
        state,
        system,
        rom) > 0);

    assert(fclose(f) == 0);
}

static void test_matching_suspended_session_closes(void)
{
    reset_root();
    make_session_dir();

    const char *rom =
        "/tmp/session-close-yellow.gb";

    write_current(
        "suspended",
        "GB",
        rom);

    assert(
        flip_session_close_suspended_game(
            "GB",
            rom) == 1);

    flip_session_state_t state;
    memset(&state, 0, sizeof(state));

    assert(
        flip_session_read_current(
            &state) == 1);

    assert(strcmp(
        state.state,
        "closed") == 0);

    assert(strcmp(
        state.system,
        "GB") == 0);

    assert(strcmp(
        state.rom,
        rom) == 0);
}

static void test_different_selected_game_does_not_close_current(void)
{
    reset_root();
    make_session_dir();

    const char *current_rom =
        "/tmp/session-close-yellow.gb";

    write_current(
        "suspended",
        "GB",
        current_rom);

    assert(
        flip_session_close_suspended_game(
            "GBC",
            "/tmp/session-close-crystal.gbc") == 0);

    flip_session_state_t state;
    memset(&state, 0, sizeof(state));

    assert(
        flip_session_read_current(
            &state) == 1);

    assert(strcmp(
        state.state,
        "suspended") == 0);

    assert(strcmp(
        state.system,
        "GB") == 0);

    assert(strcmp(
        state.rom,
        current_rom) == 0);
}

static void test_non_suspended_matching_session_is_not_closed(void)
{
    reset_root();
    make_session_dir();

    const char *rom =
        "/tmp/session-close-yellow.gb";

    write_current(
        "interrupted",
        "GB",
        rom);

    assert(
        flip_session_close_suspended_game(
            "GB",
            rom) == 0);

    flip_session_state_t state;
    memset(&state, 0, sizeof(state));

    assert(
        flip_session_read_current(
            &state) == 1);

    assert(strcmp(
        state.state,
        "interrupted") == 0);
}

int main(void)
{
    test_matching_suspended_session_closes();
    test_different_selected_game_does_not_close_current();
    test_non_suspended_matching_session_is_not_closed();

    reset_root();

    puts("target-specific session close tests passed");
    return 0;
}
