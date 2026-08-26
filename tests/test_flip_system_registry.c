#include "flip_system.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    assert(flip_system_count() == 2);

    const flip_system_t *gb =
        flip_system_find("GB");

    const flip_system_t *gbc =
        flip_system_find("GBC");

    assert(gb != NULL);
    assert(gbc != NULL);

    assert(strcmp(
        gb->rom_directory,
        "/mnt/SDCARD/Roms/GB") == 0);

    assert(strcmp(
        gbc->rom_directory,
        "/mnt/SDCARD/Roms/GBC") == 0);

    assert(strcmp(
        gb->runner,
        gbc->runner) == 0);

    assert(flip_system_supports_path(
        gb,
        "/mnt/SDCARD/Roms/GB/Tetris.gb"));

    assert(!flip_system_supports_path(
        gb,
        "/mnt/SDCARD/Roms/GB/Zelda.gbc"));

    assert(flip_system_supports_path(
        gbc,
        "/mnt/SDCARD/Roms/GBC/Zelda.gbc"));

    assert(flip_system_supports_path(
        gbc,
        "/mnt/SDCARD/Roms/GBC/ZELDA.GBC"));

    assert(!flip_system_supports_path(
        gbc,
        "/mnt/SDCARD/Roms/GBC/Tetris.gb"));

    assert(gb->session_adapter != NULL);
    assert(gbc->session_adapter != NULL);

    assert(strcmp(
        gb->session_adapter->system,
        "GB") == 0);

    assert(strcmp(
        gbc->session_adapter->system,
        "GBC") == 0);

    puts("flip_system_registry tests passed");
    return 0;
}
