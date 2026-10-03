#include "flip_session_registry.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TEST_ROOT "/tmp/flipos-session-registry-test"
#define TEST_FILE TEST_ROOT "/registry"

static void reset_registry(void)
{
    unlink(TEST_FILE);
    unlink(TEST_FILE ".tmp");
    rmdir(TEST_ROOT);
}

static flip_session_registry_entry_t make_entry(
    const char *system,
    const char *rom,
    const char *state,
    long long started,
    long long updated)
{
    flip_session_registry_entry_t entry;
    memset(&entry, 0, sizeof(entry));

    entry.valid = 1;

    snprintf(
        entry.system,
        sizeof(entry.system),
        "%s",
        system);

    snprintf(
        entry.rom,
        sizeof(entry.rom),
        "%s",
        rom);

    snprintf(
        entry.program,
        sizeof(entry.program),
        "/mnt/SDCARD/flipos-dev/flip-%s",
        strcmp(system, "GB") == 0 ? "gb" : "gba");

    snprintf(
        entry.state,
        sizeof(entry.state),
        "%s",
        state);

    entry.started = started;
    entry.updated = updated;

    return entry;
}

int main(void)
{
    reset_registry();

    flip_session_registry_entry_t entries[
        FLIP_SESSION_REGISTRY_MAX];

    size_t count = 99;

    assert(
        flip_session_registry_load(
            entries,
            FLIP_SESSION_REGISTRY_MAX,
            &count) == 0);

    assert(count == 0);

    flip_session_registry_entry_t gb =
        make_entry(
            "GB",
            "/roms/Pokemon Yellow.gb",
            "suspended",
            100,
            200);

    assert(flip_session_registry_upsert(&gb) == 0);

    memset(entries, 0, sizeof(entries));
    count = 0;

    assert(
        flip_session_registry_load(
            entries,
            FLIP_SESSION_REGISTRY_MAX,
            &count) == 0);

    assert(count == 1);
    assert(entries[0].valid == 1);
    assert(strcmp(entries[0].system, "GB") == 0);
    assert(strcmp(entries[0].rom, gb.rom) == 0);
    assert(strcmp(entries[0].state, "suspended") == 0);

    flip_session_registry_entry_t found;

    assert(
        flip_session_registry_find(
            "GB",
            gb.rom,
            &found) == 1);

    assert(found.updated == 200);

    flip_session_registry_entry_t gba =
        make_entry(
            "GBA",
            "/roms/Metroid Fusion.gba",
            "suspended",
            300,
            400);

    assert(flip_session_registry_upsert(&gba) == 0);

    count = 0;
    assert(
        flip_session_registry_load(
            entries,
            FLIP_SESSION_REGISTRY_MAX,
            &count) == 0);

    assert(count == 2);
    assert(strcmp(entries[0].system, "GBA") == 0);
    assert(strcmp(entries[1].system, "GB") == 0);

    flip_session_registry_entry_t by_state;

    assert(
        flip_session_registry_find_state(
            "suspended",
            &by_state) == 1);

    assert(strcmp(by_state.system, "GBA") == 0);
    assert(strcmp(by_state.rom, gba.rom) == 0);
    assert(by_state.updated == 400);

    assert(
        flip_session_registry_find_state(
            "running",
            &by_state) == 0);

    snprintf(
        gb.state,
        sizeof(gb.state),
        "%s",
        "interrupted");
    gb.updated = 500;

    assert(flip_session_registry_upsert(&gb) == 0);

    count = 0;
    assert(
        flip_session_registry_load(
            entries,
            FLIP_SESSION_REGISTRY_MAX,
            &count) == 0);

    assert(count == 2);
    assert(strcmp(entries[0].system, "GB") == 0);
    assert(strcmp(entries[0].state, "interrupted") == 0);
    assert(entries[0].updated == 500);
    assert(strcmp(entries[1].system, "GBA") == 0);

    assert(
        flip_session_registry_find_state(
            "suspended",
            &by_state) == 1);

    assert(strcmp(by_state.system, "GBA") == 0);
    assert(strcmp(by_state.rom, gba.rom) == 0);

    assert(
        flip_session_registry_find_state(
            "interrupted",
            &by_state) == 1);

    assert(strcmp(by_state.system, "GB") == 0);
    assert(strcmp(by_state.rom, gb.rom) == 0);

    assert(
        flip_session_registry_remove(
            "GBA",
            gba.rom) == 1);

    assert(
        flip_session_registry_remove(
            "GBA",
            gba.rom) == 0);

    count = 0;
    assert(
        flip_session_registry_load(
            entries,
            FLIP_SESSION_REGISTRY_MAX,
            &count) == 0);

    assert(count == 1);
    assert(strcmp(entries[0].system, "GB") == 0);

    assert(
        flip_session_registry_find(
            "GBA",
            gba.rom,
            &found) == 0);

    flip_session_registry_entry_t gb_blue =
        make_entry(
            "GB",
            "/roms/Pokemon Blue.gb",
            "suspended",
            600,
            700);

    flip_session_registry_entry_t gb_red =
        make_entry(
            "GB",
            "/roms/Pokemon Red.gb",
            "suspended",
            800,
            900);

    assert(flip_session_registry_upsert(&gb_blue) == 0);
    assert(flip_session_registry_upsert(&gb_red) == 0);

    assert(
        flip_session_registry_find(
            "GB",
            gb_blue.rom,
            &found) == 1);

    assert(strcmp(found.state, "suspended") == 0);

    assert(
        flip_session_registry_find(
            "GB",
            gb_red.rom,
            &found) == 1);

    assert(strcmp(found.state, "suspended") == 0);

    assert(
        flip_session_registry_find_state(
            "suspended",
            &by_state) == 1);

    assert(strcmp(by_state.rom, gb_red.rom) == 0);

    assert(
        flip_session_registry_remove(
            "GB",
            gb_blue.rom) == 1);

    assert(
        flip_session_registry_find(
            "GB",
            gb_red.rom,
            &found) == 1);

    FILE *f = fopen(TEST_FILE, "w");
    assert(f != NULL);
    fputs("version=1\nentry\nsystem=GB\nend\n", f);
    fclose(f);

    count = 0;
    assert(
        flip_session_registry_load(
            entries,
            FLIP_SESSION_REGISTRY_MAX,
            &count) == -1);

    reset_registry();

    puts("flip_session_registry tests passed");
    return 0;
}
