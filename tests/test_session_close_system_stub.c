#include "flip_system.h"

#include <string.h>

static const flip_system_t gb_system = {
    .id = "GB",
    .display_name = "Game Boy",
    .rom_directory = "/mnt/SDCARD/Roms/GB",
    .runner = "/mnt/SDCARD/flipos-dev/bin/flip-gb",
};

static const flip_system_t gbc_system = {
    .id = "GBC",
    .display_name = "Game Boy Color",
    .rom_directory = "/mnt/SDCARD/Roms/GBC",
    .runner = "/mnt/SDCARD/flipos-dev/bin/flip-gb",
};

const flip_system_t *flip_system_find(
    const char *id)
{
    if (!id)
        return NULL;

    if (strcmp(id, "GB") == 0)
        return &gb_system;

    if (strcmp(id, "GBC") == 0)
        return &gbc_system;

    return NULL;
}
