#define _GNU_SOURCE

#include "flip_session_manager.h"
#include "flip_system.h"

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

static void test_load_accepts_case_variant_system_directory(void)
{
    static const char *rom =
        "/mnt/SDCARD/Roms/GB/Pokemon - Yellow.gb";

    reset_root();

    assert(flip_session_manager_record(
        "GB",
        rom,
        FLIP_RETAINED_SESSION_SUSPENDED,
        100) == 0);

    char upper_dir[768];
    char lower_dir[768];

    int written = snprintf(
        upper_dir,
        sizeof(upper_dir),
        "%s/GB",
        FLIP_SESSION_MANAGER_ROOT);

    assert(written > 0);
    assert((size_t)written < sizeof(upper_dir));

    written = snprintf(
        lower_dir,
        sizeof(lower_dir),
        "%s/gb",
        FLIP_SESSION_MANAGER_ROOT);

    assert(written > 0);
    assert((size_t)written < sizeof(lower_dir));

    /*
     * Reproduce FAT32 preserving a lowercase directory entry while
     * canonical session metadata remains system=GB.
     */
    assert(rename(upper_dir, lower_dir) == 0);

    flip_retained_session_list_t list;
    memset(&list, 0, sizeof(list));

    assert(flip_session_manager_load(&list) == 0);
    assert(list.count == 1);
    assert(strcmp(list.items[0].system, "GB") == 0);
    assert(strcmp(list.items[0].rom, rom) == 0);

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


static void test_refresh_prunes_unsupported_system(void)
{
    static const char *rom = "/tmp/flipos-session-manager-test-rom.gb";

    reset_root();

    FILE *f = fopen(rom, "w");
    assert(f != NULL);
    assert(fputs("fixture\n", f) >= 0);
    assert(fclose(f) == 0);

    assert(flip_session_manager_record(
        "UNSUPPORTED",
        rom,
        FLIP_RETAINED_SESSION_SUSPENDED,
        100) == 0);

    flip_retained_session_list_t list;
    memset(&list, 0, sizeof(list));

    assert(flip_session_manager_refresh(&list) == 0);
    assert(list.count == 0);
    assert(list.items == NULL);

    flip_session_manager_list_free(&list);

    assert(unlink(rom) == 0);
}


static void test_refresh_prunes_missing_rom(void)
{
    static const char *rom =
        "/tmp/flipos-session-manager-definitely-missing.gb";

    reset_root();
    unlink(rom);

    assert(flip_session_manager_record(
        "GB",
        rom,
        FLIP_RETAINED_SESSION_SUSPENDED,
        200) == 0);

    flip_retained_session_list_t list;
    memset(&list, 0, sizeof(list));

    assert(flip_session_manager_refresh(&list) == 0);
    assert(list.count == 0);
    assert(list.items == NULL);

    flip_session_manager_list_free(&list);
}


static void test_refresh_prunes_unsupported_rom_path(void)
{
    static const char *rom =
        "/tmp/flipos-session-manager-wrong-extension.gbc";

    reset_root();

    FILE *f = fopen(rom, "w");
    assert(f != NULL);
    assert(fputs("fixture\n", f) >= 0);
    assert(fclose(f) == 0);

    assert(flip_session_manager_record(
        "GB",
        rom,
        FLIP_RETAINED_SESSION_SUSPENDED,
        300) == 0);

    flip_retained_session_list_t list;
    memset(&list, 0, sizeof(list));

    assert(flip_session_manager_refresh(&list) == 0);
    assert(list.count == 0);
    assert(list.items == NULL);

    flip_session_manager_list_free(&list);

    assert(unlink(rom) == 0);
}


static void test_refresh_keeps_valid_session_without_resume(void)
{
    static const char *rom =
        "/tmp/flipos-session-manager-no-resume.gb";

    reset_root();

    FILE *f = fopen(rom, "w");
    assert(f != NULL);
    assert(fputs("fixture\n", f) >= 0);
    assert(fclose(f) == 0);

    assert(flip_session_manager_record(
        "GB",
        rom,
        FLIP_RETAINED_SESSION_SUSPENDED,
        400) == 0);

    flip_retained_session_list_t list;
    memset(&list, 0, sizeof(list));

    assert(flip_session_manager_refresh(&list) == 0);
    assert(list.count == 1);
    assert(list.items != NULL);
    assert(list.items[0].resume_available == 0);

    flip_session_manager_list_free(&list);
    assert(unlink(rom) == 0);
}

static void test_refresh_derives_resume_availability(void)
{
    static const char *rom =
        "/tmp/flipos-session-manager-has-resume.gb";

    reset_root();

    FILE *f = fopen(rom, "w");
    assert(f != NULL);
    assert(fputs("fixture\n", f) >= 0);
    assert(fclose(f) == 0);

    assert(flip_session_manager_record(
        "GB",
        rom,
        FLIP_RETAINED_SESSION_SUSPENDED,
        500) == 0);

    flip_retained_session_list_t list;
    memset(&list, 0, sizeof(list));

    assert(flip_session_manager_refresh(&list) == 0);
    assert(list.count == 1);
    assert(list.items != NULL);
    assert(list.items[0].resume_available == 1);

    flip_session_manager_list_free(&list);
    assert(unlink(rom) == 0);
}


static void test_refresh_deduplicates_same_system_and_rom(void)
{
    static const char *rom =
        "/tmp/flipos-session-manager-duplicate.gb";

    reset_root();

    FILE *f = fopen(rom, "w");
    assert(f != NULL);
    assert(fputs("fixture\n", f) >= 0);
    assert(fclose(f) == 0);

    assert(flip_session_manager_record(
        "GB",
        rom,
        FLIP_RETAINED_SESSION_SUSPENDED,
        600) == 0);

    char original[1024];
    session_path(
        original,
        sizeof(original),
        "GB",
        rom);

    char duplicate[1024];
    int written = snprintf(
        duplicate,
        sizeof(duplicate),
        "%s/GB/duplicate.session",
        FLIP_SESSION_MANAGER_ROOT);

    assert(written > 0);
    assert((size_t)written < sizeof(duplicate));

    FILE *src = fopen(original, "r");
    assert(src != NULL);

    FILE *dst = fopen(duplicate, "w");
    assert(dst != NULL);

    char buffer[256];
    size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), src)) > 0)
        assert(fwrite(buffer, 1, n, dst) == n);

    assert(ferror(src) == 0);
    assert(fclose(src) == 0);
    assert(fclose(dst) == 0);

    flip_retained_session_list_t list;
    memset(&list, 0, sizeof(list));

    assert(flip_session_manager_refresh(&list) == 0);
    assert(list.count == 1);
    assert(list.items != NULL);
    assert(strcmp(list.items[0].system, "GB") == 0);
    assert(strcmp(list.items[0].rom, rom) == 0);

    flip_session_manager_list_free(&list);

    assert(unlink(rom) == 0);
}





static void test_prune_preserves_case_variant_canonical_session(void)
{
    static const char *rom =
        "/tmp/flipos-session-manager-case-prune.gb";

    reset_root();

    FILE *rom_file = fopen(
        rom,
        "w");

    assert(rom_file != NULL);
    assert(fputs(
        "fixture\n",
        rom_file) >= 0);
    assert(fclose(rom_file) == 0);

    /*
     * Record canonically as GB first.
     */
    assert(
        flip_session_manager_record(
            "GB",
            rom,
            FLIP_RETAINED_SESSION_SUSPENDED,
            850) == 0);

    char upper_dir[1024];
    char lower_dir[1024];

    int written = snprintf(
        upper_dir,
        sizeof(upper_dir),
        "%s/GB",
        FLIP_SESSION_MANAGER_ROOT);

    assert(written > 0);
    assert((size_t)written < sizeof(upper_dir));

    written = snprintf(
        lower_dir,
        sizeof(lower_dir),
        "%s/gb",
        FLIP_SESSION_MANAGER_ROOT);

    assert(written > 0);
    assert((size_t)written < sizeof(lower_dir));

    /*
     * Reproduce the Flip FAT32 directory spelling seen on hardware:
     * metadata system=GB, directory entry preserved as lowercase gb.
     */
    assert(rename(
        upper_dir,
        lower_dir) == 0);

    char expected[1024];

    written = snprintf(
        expected,
        sizeof(expected),
        "%s/gb/%016llx.session",
        FLIP_SESSION_MANAGER_ROOT,
        (unsigned long long)fnv1a64(rom));

    assert(written > 0);
    assert((size_t)written < sizeof(expected));

    assert(access(
        expected,
        F_OK) == 0);

    /*
     * This is a healthy canonical session. Directory case alone
     * must never make pruning delete it.
     */
    assert(
        flip_session_manager_prune() == 0);

    assert(access(
        expected,
        F_OK) == 0);

    assert(unlink(
        rom) == 0);
}

static void test_prune_removes_wrong_hash_session_file(void)
{
    static const char *rom =
        "/tmp/flipos-session-manager-wrong-hash.gb";

    reset_root();

    FILE *rom_file = fopen(
        rom,
        "w");

    assert(rom_file != NULL);
    assert(fputs(
        "fixture\n",
        rom_file) >= 0);
    assert(fclose(rom_file) == 0);

    char system_dir[1024];

    int written = snprintf(
        system_dir,
        sizeof(system_dir),
        "%s/GB",
        FLIP_SESSION_MANAGER_ROOT);

    assert(written > 0);
    assert((size_t)written < sizeof(system_dir));

    assert(mkdir(
        FLIP_SESSION_MANAGER_ROOT,
        0777) == 0);

    assert(mkdir(
        system_dir,
        0777) == 0);

    /*
     * Valid session metadata deliberately stored under the
     * wrong filename hash.
     */
    char wrong_path[1024];

    written = snprintf(
        wrong_path,
        sizeof(wrong_path),
        "%s/GB/0000000000000001.session",
        FLIP_SESSION_MANAGER_ROOT);

    assert(written > 0);
    assert((size_t)written < sizeof(wrong_path));

    FILE *f = fopen(
        wrong_path,
        "w");

    assert(f != NULL);

    assert(fprintf(
        f,
        "version=1\n"
        "system=GB\n"
        "rom=%s\n"
        "status=suspended\n"
        "last_active=900\n",
        rom) > 0);

    assert(fclose(f) == 0);

    assert(access(
        wrong_path,
        F_OK) == 0);

    /*
     * Prune must reject orphan metadata whose filename does not
     * match the canonical ROM hash.
     */
    assert(
        flip_session_manager_prune() == 0);

    assert(access(
        wrong_path,
        F_OK) != 0);

    assert(errno == ENOENT);

    assert(unlink(
        rom) == 0);
}

static void test_prune_removes_malformed_session_file(void)
{
    reset_root();

    char system_dir[1024];

    int written = snprintf(
        system_dir,
        sizeof(system_dir),
        "%s/GB",
        FLIP_SESSION_MANAGER_ROOT);

    assert(written > 0);
    assert((size_t)written < sizeof(system_dir));

    assert(mkdir(
        FLIP_SESSION_MANAGER_ROOT,
        0777) == 0);

    assert(mkdir(
        system_dir,
        0777) == 0);

    char malformed[1024];

    written = snprintf(
        malformed,
        sizeof(malformed),
        "%s/broken.session",
        system_dir);

    assert(written > 0);
    assert((size_t)written < sizeof(malformed));

    FILE *f = fopen(
        malformed,
        "w");

    assert(f != NULL);

    assert(fputs(
        "this-is-not-valid-session-metadata\n",
        f) >= 0);

    assert(fclose(f) == 0);

    assert(access(
        malformed,
        F_OK) == 0);

    /*
     * Normal loading may ignore malformed metadata, but explicit
     * pruning must remove it from persistent session storage.
     */
    assert(
        flip_session_manager_prune() == 0);

    assert(access(
        malformed,
        F_OK) != 0);

    assert(errno == ENOENT);
}

static void test_prune_removes_invalid_metadata(void)
{
    static const char *missing_rom =
        "/tmp/flipos-session-manager-prune-missing.gb";

    reset_root();
    unlink(missing_rom);

    assert(flip_session_manager_record(
        "GB",
        missing_rom,
        FLIP_RETAINED_SESSION_SUSPENDED,
        700) == 0);

    assert(flip_session_manager_prune() == 0);

    flip_retained_session_list_t list;
    memset(&list, 0, sizeof(list));

    assert(flip_session_manager_load(&list) == 0);
    assert(list.count == 0);
    assert(list.items == NULL);

    flip_session_manager_list_free(&list);
}


static void test_refresh_reports_unreadable_system_directory(void)
{
    static const char *rom =
        "/tmp/flipos-session-manager-unreadable-dir.gb";

    reset_root();

    FILE *f = fopen(rom, "w");
    assert(f != NULL);
    assert(fputs("fixture\n", f) >= 0);
    assert(fclose(f) == 0);

    assert(flip_session_manager_record(
        "GB",
        rom,
        FLIP_RETAINED_SESSION_SUSPENDED,
        800) == 0);

    char system_dir[1024];
    int written = snprintf(
        system_dir,
        sizeof(system_dir),
        "%s/GB",
        FLIP_SESSION_MANAGER_ROOT);

    assert(written > 0);
    assert((size_t)written < sizeof(system_dir));

    assert(chmod(system_dir, 0000) == 0);

    flip_retained_session_list_t list;
    memset(&list, 0, sizeof(list));

    assert(flip_session_manager_refresh(&list) == -1);
    assert(list.count == 0);
    assert(list.items == NULL);

    assert(chmod(system_dir, 0777) == 0);

    flip_session_manager_list_free(&list);

    assert(unlink(rom) == 0);
}


static void test_record_gameswitcher_uses_suspended_status(void)
{
    static const char *rom =
        "/tmp/flipos-session-manager-gameswitcher.gb";

    reset_root();

    FILE *f = fopen(rom, "w");
    assert(f != NULL);
    assert(fputs("fixture\n", f) >= 0);
    assert(fclose(f) == 0);

    const flip_system_t *system =
        flip_system_find("GB");

    assert(system != NULL);

    assert(
        flip_session_manager_record_gameswitcher(
            system,
            rom) == 0);

    flip_retained_session_list_t list;
    memset(&list, 0, sizeof(list));

    assert(flip_session_manager_load(&list) == 0);
    assert(list.count == 1);
    assert(list.items != NULL);
    assert(strcmp(list.items[0].system, "GB") == 0);
    assert(strcmp(list.items[0].rom, rom) == 0);
    assert(
        list.items[0].status ==
        FLIP_RETAINED_SESSION_SUSPENDED);
    assert(list.items[0].last_active > 0);

    flip_session_manager_list_free(&list);

    assert(unlink(rom) == 0);
}

static void test_record_gameswitcher_rejects_invalid_input(void)
{
    const flip_system_t *system =
        flip_system_find("GB");

    assert(system != NULL);

    assert(
        flip_session_manager_record_gameswitcher(
            NULL,
            "/tmp/test.gb") == -1);

    assert(
        flip_session_manager_record_gameswitcher(
            system,
            NULL) == -1);

    assert(
        flip_session_manager_record_gameswitcher(
            system,
            "") == -1);
}


static void test_touch_moves_session_to_most_recent(void)
{
    const char *older =
        "/tmp/flipos-session-manager-touch-older.gb";

    const char *newer =
        "/tmp/flipos-session-manager-touch-newer.gb";

    FILE *fp = fopen(older, "wb");
    assert(fp != NULL);
    fclose(fp);

    fp = fopen(newer, "wb");
    assert(fp != NULL);
    fclose(fp);

    assert(
        flip_session_manager_record(
            "GB",
            older,
            FLIP_RETAINED_SESSION_SUSPENDED,
            100) == 0);

    assert(
        flip_session_manager_record(
            "GB",
            newer,
            FLIP_RETAINED_SESSION_SUSPENDED,
            200) == 0);

    assert(
        flip_session_manager_touch(
            "GB",
            older) == 0);

    flip_retained_session_list_t list;
    memset(&list, 0, sizeof(list));

    assert(
        flip_session_manager_load(
            &list) == 0);

    assert(list.count >= 2);

    assert(
        strcmp(
            list.items[0].rom,
            older) == 0);

    assert(
        list.items[0].last_active >
        list.items[1].last_active);

    flip_session_manager_list_free(
        &list);

    unlink(older);
    unlink(newer);
}


static void test_remove_preserves_other_retained_session(void)
{
    const char *first =
        "/tmp/flipos-session-manager-remove-first.gb";

    const char *second =
        "/tmp/flipos-session-manager-remove-second.gb";

    reset_root();

    assert(
        flip_session_manager_record(
            "GB",
            first,
            FLIP_RETAINED_SESSION_SUSPENDED,
            100) == 0);

    assert(
        flip_session_manager_record(
            "GB",
            second,
            FLIP_RETAINED_SESSION_SUSPENDED,
            200) == 0);

    assert(
        flip_session_manager_remove(
            "GB",
            first) == 1);

    flip_retained_session_list_t list;
    memset(&list, 0, sizeof(list));

    assert(
        flip_session_manager_load(
            &list) == 0);

    assert(list.count == 1);

    assert(
        strcmp(
            list.items[0].rom,
            second) == 0);

    flip_session_manager_list_free(
        &list);
}

int main(void)
{
    test_remove_preserves_other_retained_session();
    test_touch_moves_session_to_most_recent();
    test_empty_registry();
    test_record_round_trip_and_frozen_identity();
    test_load_accepts_case_variant_system_directory();
    test_update_replaces_without_duplicate();
    test_more_than_ten_sessions_and_recent_order();
    test_remove_only_target_session();
    test_malformed_file_does_not_hide_valid_session();
    test_failed_update_preserves_committed_session();
    test_refresh_prunes_unsupported_system();
    test_refresh_prunes_missing_rom();
    test_refresh_prunes_unsupported_rom_path();
    test_refresh_keeps_valid_session_without_resume();
    test_refresh_derives_resume_availability();
    test_refresh_deduplicates_same_system_and_rom();
    test_prune_preserves_case_variant_canonical_session();
    test_prune_removes_wrong_hash_session_file();
    test_prune_removes_malformed_session_file();
    test_prune_removes_invalid_metadata();
    test_refresh_reports_unreadable_system_directory();
    test_record_gameswitcher_uses_suspended_status();
    test_record_gameswitcher_rejects_invalid_input();

    reset_root();
    return 0;
}
