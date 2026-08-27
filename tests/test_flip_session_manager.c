#define _GNU_SOURCE

#include "flip_session_manager.h"

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef FLIP_SESSION_MANAGER_ROOT
#define FLIP_SESSION_MANAGER_ROOT "/tmp/flipos-session-manager-test"
#endif

static uint64_t fnv1a64(const char *text)
{
    uint64_t hash = UINT64_C(14695981039346656037);

    while (text && *text) {
        hash ^= (unsigned char)*text++;
        hash *= UINT64_C(1099511628211);
    }

    return hash;
}

static void reset_root(void)
{
    char command[768];
    int written = snprintf(
        command,
        sizeof(command),
        "rm -rf '%s'",
        FLIP_SESSION_MANAGER_ROOT);

    assert(written > 0);
    assert((size_t)written < sizeof(command));
    assert(system(command) == 0);
}

static void session_path(
    char *out,
    size_t out_size,
    const char *system,
    const char *rom)
{
    int written = snprintf(
        out,
        out_size,
        "%s/%s/%016llx.session",
        FLIP_SESSION_MANAGER_ROOT,
        system,
        (unsigned long long)fnv1a64(rom));

    assert(written > 0);
    assert((size_t)written < out_size);
}

static void test_empty_registry(void)
{
    reset_root();

    flip_retained_session_list_t list;
    memset(&list, 0, sizeof(list));

    assert(flip_session_manager_load(&list) == 0);
    assert(list.count == 0);
    assert(list.items == NULL);

    flip_session_manager_list_free(&list);
}

static void test_record_round_trip_and_frozen_identity(void)
{
    static const char *rom =
        "/mnt/SDCARD/Roms/GB/Pokemon - Yellow.gb";

    reset_root();

    assert(fnv1a64(rom) == UINT64_C(0x792f9628ba43b7d9));

    assert(flip_session_manager_record(
        "GB",
        rom,
        FLIP_RETAINED_SESSION_SUSPENDED,
        100) == 0);

    char path[768];
    session_path(path, sizeof(path), "GB", rom);
    assert(access(path, R_OK) == 0);

    flip_retained_session_list_t list;
    memset(&list, 0, sizeof(list));

    assert(flip_session_manager_load(&list) == 0);
    assert(list.count == 1);
    assert(strcmp(list.items[0].system, "GB") == 0);
    assert(strcmp(list.items[0].rom, rom) == 0);
    assert(list.items[0].status == FLIP_RETAINED_SESSION_SUSPENDED);
    assert(list.items[0].last_active == 100);

    flip_session_manager_list_free(&list);
}

static void test_update_replaces_without_duplicate(void)
{
    static const char *rom = "/roms/game.gb";

    reset_root();

    assert(flip_session_manager_record(
        "GB", rom, FLIP_RETAINED_SESSION_SUSPENDED, 10) == 0);
    assert(flip_session_manager_record(
        "GB", rom, FLIP_RETAINED_SESSION_INTERRUPTED, 20) == 0);

    flip_retained_session_list_t list;
    memset(&list, 0, sizeof(list));

    assert(flip_session_manager_load(&list) == 0);
    assert(list.count == 1);
    assert(list.items[0].status == FLIP_RETAINED_SESSION_INTERRUPTED);
    assert(list.items[0].last_active == 20);

    flip_session_manager_list_free(&list);
}

static void test_more_than_ten_sessions_and_recent_order(void)
{
    reset_root();

    for (int i = 0; i < 12; ++i) {
        char rom[128];
        snprintf(rom, sizeof(rom), "/roms/game-%02d.gb", i);

        assert(flip_session_manager_record(
            "GB",
            rom,
            FLIP_RETAINED_SESSION_SUSPENDED,
            1000 + i) == 0);
    }

    flip_retained_session_list_t list;
    memset(&list, 0, sizeof(list));

    assert(flip_session_manager_load(&list) == 0);
    assert(list.count == 12);
    assert(strcmp(list.items[0].rom, "/roms/game-11.gb") == 0);
    assert(strcmp(list.items[11].rom, "/roms/game-00.gb") == 0);

    for (size_t i = 1; i < list.count; ++i)
        assert(list.items[i - 1].last_active >= list.items[i].last_active);

    flip_session_manager_list_free(&list);
}

static void test_remove_only_target_session(void)
{
    static const char *rom_a = "/roms/a.gb";
    static const char *rom_b = "/roms/b.gb";

    reset_root();

    assert(flip_session_manager_record(
        "GB", rom_a, FLIP_RETAINED_SESSION_SUSPENDED, 1) == 0);
    assert(flip_session_manager_record(
        "GB", rom_b, FLIP_RETAINED_SESSION_SUSPENDED, 2) == 0);

    assert(flip_session_manager_remove("GB", rom_a) == 1);
    assert(flip_session_manager_remove("GB", rom_a) == 0);

    flip_retained_session_list_t list;
    memset(&list, 0, sizeof(list));

    assert(flip_session_manager_load(&list) == 0);
    assert(list.count == 1);
    assert(strcmp(list.items[0].rom, rom_b) == 0);

    flip_session_manager_list_free(&list);
}

static void test_malformed_file_does_not_hide_valid_session(void)
{
    static const char *valid_rom = "/roms/valid.gb";

    reset_root();

    assert(flip_session_manager_record(
        "GB", valid_rom, FLIP_RETAINED_SESSION_SUSPENDED, 3) == 0);

    char dir[768];
    snprintf(dir, sizeof(dir), "%s/GB", FLIP_SESSION_MANAGER_ROOT);

    char malformed[1024];
    snprintf(malformed, sizeof(malformed), "%s/broken.session", dir);

    FILE *f = fopen(malformed, "w");
    assert(f != NULL);
    assert(fputs("not-a-session\n", f) >= 0);
    assert(fclose(f) == 0);

    flip_retained_session_list_t list;
    memset(&list, 0, sizeof(list));

    assert(flip_session_manager_load(&list) == 0);
    assert(list.count == 1);
    assert(strcmp(list.items[0].rom, valid_rom) == 0);

    flip_session_manager_list_free(&list);
}

static void test_failed_update_preserves_committed_session(void)
{
    static const char *rom = "/roms/protected.gb";

    reset_root();

    assert(flip_session_manager_record(
        "GB", rom, FLIP_RETAINED_SESSION_SUSPENDED, 10) == 0);

    char dir[768];
    snprintf(dir, sizeof(dir), "%s/GB", FLIP_SESSION_MANAGER_ROOT);

    assert(chmod(dir, 0555) == 0);

    int rc = flip_session_manager_record(
        "GB", rom, FLIP_RETAINED_SESSION_INTERRUPTED, 99);

    assert(chmod(dir, 0755) == 0);
    assert(rc < 0);

    flip_retained_session_list_t list;
    memset(&list, 0, sizeof(list));

    assert(flip_session_manager_load(&list) == 0);
    assert(list.count == 1);
    assert(list.items[0].status == FLIP_RETAINED_SESSION_SUSPENDED);
    assert(list.items[0].last_active == 10);

    flip_session_manager_list_free(&list);
}

int main(void)
{
    test_empty_registry();
    test_record_round_trip_and_frozen_identity();
    test_update_replaces_without_duplicate();
    test_more_than_ten_sessions_and_recent_order();
    test_remove_only_target_session();
    test_malformed_file_does_not_hide_valid_session();
    test_failed_update_preserves_committed_session();

    reset_root();
    return 0;
}
