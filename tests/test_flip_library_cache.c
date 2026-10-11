#include "flip_library.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define TEST_ROOT "/tmp/flipos-library-service-test"
#define TEST_ROMS TEST_ROOT "/roms"
#define TEST_CACHE TEST_ROOT "/cache/library"

int flip_system_supports_path(
    const flip_system_t *system,
    const char *path)
{
    if (!system || !path)
        return 0;

    const char *dot = strrchr(path, '.');
    if (!dot)
        return 0;

    for (int i = 0; i < system->rom_extension_count; ++i) {
        if (strcasecmp(dot, system->rom_extensions[i]) == 0)
            return 1;
    }

    return 0;
}

static void remove_file(const char *path)
{
    unlink(path);
}

static void cleanup(void)
{
    remove_file(TEST_ROMS "/Alpha.gb");
    remove_file(TEST_ROMS "/Beta.gb");
    remove_file(TEST_ROMS "/Gamma.gb");
    remove_file(TEST_ROMS "/notes.txt");

    remove_file(TEST_CACHE "/GB.fingerprint");
    remove_file(TEST_CACHE "/GB.fingerprint.tmp");
    remove_file(TEST_CACHE "/GB.index");
    remove_file(TEST_CACHE "/GB.index.tmp");

    rmdir(TEST_CACHE);
    rmdir(TEST_ROOT "/cache");
    rmdir(TEST_ROMS);
    rmdir(TEST_ROOT);
}

static void ensure_dir(const char *path)
{
    if (mkdir(path, 0700) < 0)
        assert(errno == EEXIST);
}

static void setup(void)
{
    cleanup();
    ensure_dir(TEST_ROOT);
    ensure_dir(TEST_ROMS);
}

static void write_rom(const char *path, const char *payload)
{
    FILE *f = fopen(path, "wb");
    assert(f != NULL);
    assert(fwrite(payload, 1, strlen(payload), f) == strlen(payload));
    assert(fclose(f) == 0);
}

static void write_text(const char *path, const char *text)
{
    FILE *f = fopen(path, "w");
    assert(f != NULL);
    assert(fputs(text, f) >= 0);
    assert(fclose(f) == 0);
}

int main(void)
{
    static const char *const extensions[] = {
        ".gb"
    };

    const flip_system_t system = {
        .id = "GB",
        .display_name = "Game Boy",
        .runner = "flip-gb",
        .rom_directory = TEST_ROMS,
        .rom_extensions = extensions,
        .rom_extension_count = 1,
        .session_adapter = NULL,
    };

    setup();

    write_rom(TEST_ROMS "/Beta.gb", "beta");
    write_rom(TEST_ROMS "/Alpha.gb", "alpha");
    write_text(TEST_ROMS "/notes.txt", "ignored");

    flip_library_cache_status_t status;

    assert(flip_library_cache_evaluate(
        &system,
        &status) == 0);

    assert(status.state == FLIP_LIBRARY_CACHE_MISS);
    assert(status.live_rom_count == 2);

    const char *const names[] = {
        "Alpha.gb",
        "Beta.gb"
    };

    assert(flip_library_commit_generation(
        &system,
        names,
        2) == 0);

    assert(flip_library_cache_evaluate(
        &system,
        &status) == 0);

    assert(status.state == FLIP_LIBRARY_CACHE_HIT);
    assert(status.live_rom_count == 2);
    assert(status.cached_rom_count == 2);

    write_rom(TEST_ROMS "/Gamma.gb", "gamma");

    assert(flip_library_cache_evaluate(
        &system,
        &status) == 0);

    assert(status.state == FLIP_LIBRARY_CACHE_CHANGED);
    assert(status.live_rom_count == 3);
    assert(status.cached_rom_count == 2);

    remove_file(TEST_ROMS "/Gamma.gb");

    assert(flip_library_cache_evaluate(
        &system,
        &status) == 0);
    assert(status.state == FLIP_LIBRARY_CACHE_HIT);

    write_text(
        TEST_CACHE "/GB.index",
        "version=1\nrom_count=2\nrom=Alpha.gb\n");

    assert(flip_library_cache_evaluate(
        &system,
        &status) == 0);

    assert(status.state == FLIP_LIBRARY_CACHE_MISS);
    assert(status.live_rom_count == 2);

    char title[256];
    assert(flip_library_clean_display_title(
        "Pokemon - FireRed Version (USA, Europe) (Rev 1).gba",
        title, sizeof(title)) == 0);
    assert(strcmp(title, "Pokemon - FireRed Version") == 0);

    assert(flip_library_clean_display_title(
        "Golden Sun - The Lost Age (USA, Europe).gba",
        title, sizeof(title)) == 0);
    assert(strcmp(title, "Golden Sun - The Lost Age") == 0);

    assert(flip_library_clean_display_title(
        "Mother 3 (Tr).gba", title, sizeof(title)) == 0);
    assert(strcmp(title, "Mother 3") == 0);

    assert(flip_library_clean_display_title(
        "Example Game (Special Edition) (USA) [!].gb",
        title, sizeof(title)) == 0);
    assert(strcmp(title, "Example Game (Special Edition)") == 0);

    assert(flip_library_clean_display_title(
        "Game [h1].gb", title, sizeof(title)) == 0);
    assert(strcmp(title, "Game") == 0);

    assert(flip_library_clean_display_title(
        "Game (Anniversary Edition).gb", title, sizeof(title)) == 0);
    assert(strcmp(title, "Game (Anniversary Edition)") == 0);

    cleanup();

    puts("flip_library cache service tests passed");
    return 0;
}
