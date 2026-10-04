#include "flip_bios.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TEST_ROOT FLIP_SYSTEM_ROOT
#define TEST_GB_DIR FLIP_SYSTEM_ROOT "/gb"
#define TEST_GBA_DIR FLIP_SYSTEM_ROOT "/gba"

static void cleanup_fixture(void)
{
    unlink(TEST_GB_DIR "/gb_bios.bin");
    unlink(TEST_GB_DIR "/gbc_bios.bin");
    unlink(TEST_GBA_DIR "/gba_bios.bin");
    rmdir(TEST_GB_DIR "/gb_bios.bin");
    rmdir(TEST_GB_DIR "/gbc_bios.bin");
    rmdir(TEST_GBA_DIR "/gba_bios.bin");
    rmdir(TEST_GB_DIR);
    rmdir(TEST_GBA_DIR);
    rmdir(TEST_ROOT);
}

static void ensure_fixture(void)
{
    cleanup_fixture();

    if (mkdir(TEST_ROOT, 0700) < 0)
        assert(errno == EEXIST);

    if (mkdir(TEST_GB_DIR, 0700) < 0)
        assert(errno == EEXIST);

    if (mkdir(TEST_GBA_DIR, 0700) < 0)
        assert(errno == EEXIST);
}

static void write_sized_file(const char *path, size_t size)
{
    FILE *file = fopen(path, "wb");
    assert(file != NULL);

    unsigned char zero = 0;
    for (size_t i = 0; i < size; ++i)
        assert(fwrite(&zero, 1, 1, file) == 1);

    assert(fclose(file) == 0);
}

static void test_system_directory(void)
{
    char path[FLIP_BIOS_PATH_MAX];

    assert(flip_bios_system_directory(
        "GB",
        path,
        sizeof(path)) == 0);

    assert(strcmp(path, TEST_GB_DIR) == 0);

    assert(flip_bios_system_directory(
        "GBA",
        path,
        sizeof(path)) == 0);

    assert(strcmp(path, TEST_GBA_DIR) == 0);

    assert(flip_bios_system_directory(
        "UNKNOWN",
        path,
        sizeof(path)) < 0);

    assert(flip_bios_system_directory(
        "GB",
        NULL,
        sizeof(path)) < 0);
}

static void test_missing_bioses(void)
{
    flip_bios_info_t info;

    assert(flip_bios_query(
        FLIP_BIOS_GB_DMG,
        &info) == 0);

    assert(info.status == FLIP_BIOS_MISSING);
    assert(strcmp(info.system_id, "GB") == 0);
    assert(strcmp(info.filename, "gb_bios.bin") == 0);
    assert(info.expected_size == 256);
    assert(info.actual_size == 0);
    assert(strcmp(
        info.path,
        TEST_GB_DIR "/gb_bios.bin") == 0);

    assert(flip_bios_query(
        FLIP_BIOS_GB_CGB,
        &info) == 0);

    assert(info.status == FLIP_BIOS_MISSING);
    assert(strcmp(info.filename, "gbc_bios.bin") == 0);
    assert(info.expected_size == 2304);
    assert(info.actual_size == 0);

    assert(flip_bios_query(
        FLIP_BIOS_GBA,
        &info) == 0);

    assert(info.status == FLIP_BIOS_MISSING);
    assert(strcmp(info.system_id, "GBA") == 0);
    assert(strcmp(info.filename, "gba_bios.bin") == 0);
    assert(info.expected_size == 16384);
    assert(info.actual_size == 0);
    assert(strcmp(
        info.path,
        TEST_GBA_DIR "/gba_bios.bin") == 0);
}

static void test_valid_exact_sizes(void)
{
    flip_bios_info_t info;

    write_sized_file(
        TEST_GB_DIR "/gb_bios.bin",
        256);

    assert(flip_bios_query(
        FLIP_BIOS_GB_DMG,
        &info) == 0);

    assert(info.status == FLIP_BIOS_VALID);
    assert(info.actual_size == 256);

    write_sized_file(
        TEST_GB_DIR "/gbc_bios.bin",
        2304);

    assert(flip_bios_query(
        FLIP_BIOS_GB_CGB,
        &info) == 0);

    assert(info.status == FLIP_BIOS_VALID);
    assert(info.actual_size == 2304);

    write_sized_file(
        TEST_GBA_DIR "/gba_bios.bin",
        16384);

    assert(flip_bios_query(
        FLIP_BIOS_GBA,
        &info) == 0);

    assert(info.status == FLIP_BIOS_VALID);
    assert(info.actual_size == 16384);
}

static void test_wrong_sizes(void)
{
    flip_bios_info_t info;

    write_sized_file(
        TEST_GB_DIR "/gb_bios.bin",
        255);

    assert(flip_bios_query(
        FLIP_BIOS_GB_DMG,
        &info) == 0);

    assert(info.status == FLIP_BIOS_INVALID);
    assert(info.actual_size == 255);

    unlink(TEST_GB_DIR "/gb_bios.bin");

    write_sized_file(
        TEST_GB_DIR "/gb_bios.bin",
        257);

    assert(flip_bios_query(
        FLIP_BIOS_GB_DMG,
        &info) == 0);

    assert(info.status == FLIP_BIOS_INVALID);
    assert(info.actual_size == 257);
}

static void test_directory_is_invalid(void)
{
    flip_bios_info_t info;

    unlink(TEST_GB_DIR "/gb_bios.bin");
    assert(mkdir(
        TEST_GB_DIR "/gb_bios.bin",
        0700) == 0);

    assert(flip_bios_query(
        FLIP_BIOS_GB_DMG,
        &info) == 0);

    assert(info.status == FLIP_BIOS_INVALID);
    assert(info.actual_size == 0);
}

static void test_invalid_arguments(void)
{
    flip_bios_info_t info;

    assert(flip_bios_query(
        (flip_bios_kind_t)999,
        &info) < 0);

    assert(flip_bios_query(
        FLIP_BIOS_GB_DMG,
        NULL) < 0);
}

int main(void)
{
    ensure_fixture();

    test_system_directory();
    test_missing_bioses();

    cleanup_fixture();
    ensure_fixture();
    test_valid_exact_sizes();

    cleanup_fixture();
    ensure_fixture();
    test_wrong_sizes();

    cleanup_fixture();
    ensure_fixture();
    test_directory_is_invalid();

    test_invalid_arguments();

    cleanup_fixture();

    puts("flip_bios tests passed");
    return 0;
}
