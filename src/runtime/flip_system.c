#include "flip_system.h"

#include <stddef.h>
#include <string.h>
#include <strings.h>

static const char *const gb_extensions[] = {
    ".gb"
};

static const char *const gbc_extensions[] = {
    ".gbc"
};

static const char *const gba_extensions[] = {
    ".gba"
};

static flip_system_t systems[] = {
    {
        .id = "GB",
        .display_name = "Game Boy",

        .runner =
            "/mnt/SDCARD/flipos-dev/bin/flip-gb",

        .rom_directory =
            "/mnt/SDCARD/Roms/GB",

        .rom_extensions =
            gb_extensions,

        .rom_extension_count =
            (int)(
                sizeof(gb_extensions) /
                sizeof(gb_extensions[0])),

        .session_adapter = NULL
    },
    {
        .id = "GBC",
        .display_name = "Game Boy Color",

        .runner =
            "/mnt/SDCARD/flipos-dev/bin/flip-gb",

        .rom_directory =
            "/mnt/SDCARD/Roms/GBC",

        .rom_extensions =
            gbc_extensions,

        .rom_extension_count =
            (int)(
                sizeof(gbc_extensions) /
                sizeof(gbc_extensions[0])),

        .session_adapter = NULL
    },
    {
        .id = "GBA",
        .display_name = "Game Boy Advance",

        .runner =
            "/mnt/SDCARD/flipos-dev/bin/flip-gba",

        .rom_directory =
            "/mnt/SDCARD/Roms/GBA",

        .rom_extensions =
            gba_extensions,

        .rom_extension_count =
            (int)(
                sizeof(gba_extensions) /
                sizeof(gba_extensions[0])),

        .session_adapter = NULL
    }
};

#define SYSTEM_COUNT \
    ((int)(sizeof(systems) / sizeof(systems[0])))

/*
 * Resolve runtime-owned fields for a registry entry.
 *
 * Keeping this in one place means callers do not need
 * to know how session adapters are attached.
 */
static const flip_system_t *
prepare_system(
    flip_system_t *system)
{
    if (!system)
        return NULL;

    if (!system->session_adapter) {

        system->session_adapter =
            flip_session_adapter_for_system(
                system->id);
    }

    return system;
}

const flip_system_t *
flip_system_find(
    const char *id)
{
    if (!id)
        return NULL;

    for (int i = 0;
         i < SYSTEM_COUNT;
         ++i) {

        if (strcmp(
                systems[i].id,
                id) != 0) {

            continue;
        }

        return prepare_system(
            &systems[i]);
    }

    return NULL;
}

int flip_system_resume_available(
    const flip_system_t *system,
    const char *rom_path)
{
    if (!system ||
        !system->session_adapter ||
        !system->session_adapter->resume_available)
        return 0;

    return system->session_adapter->resume_available(
        rom_path);
}

int flip_system_supports_path(
    const flip_system_t *system,
    const char *path)
{
    if (!system ||
        !path ||
        !path[0] ||
        !system->rom_extensions ||
        system->rom_extension_count <= 0)
        return 0;

    const char *dot =
        strrchr(
            path,
            '.');

    if (!dot)
        return 0;

    for (int i = 0;
         i < system->rom_extension_count;
         ++i) {

        const char *extension =
            system->rom_extensions[i];

        if (extension &&
            strcasecmp(
                dot,
                extension) == 0) {

            return 1;
        }
    }

    return 0;
}

int flip_system_count(void)
{
    return SYSTEM_COUNT;
}

const flip_system_t *
flip_system_at(
    int index)
{
    if (index < 0 ||
        index >= SYSTEM_COUNT)
        return NULL;

    return prepare_system(
        &systems[index]);
}

const flip_system_t *
flip_system_default(void)
{
    if (SYSTEM_COUNT <= 0)
        return NULL;

    return flip_system_at(0);
}
