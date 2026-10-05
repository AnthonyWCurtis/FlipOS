#include "flip_bios.h"
#include "flip_paths.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

typedef struct {
    flip_bios_kind_t kind;
    const char *system_id;
    const char *filename;
    size_t expected_size;
} flip_bios_definition_t;

static const flip_bios_definition_t BIOS_DEFINITIONS[] = {
    {
        .kind = FLIP_BIOS_GB_DMG,
        .system_id = "GB",
        .filename = "gb_bios.bin",
        .expected_size = 256,
    },
    {
        .kind = FLIP_BIOS_GB_CGB,
        .system_id = "GB",
        .filename = "gbc_bios.bin",
        .expected_size = 2304,
    },
    {
        .kind = FLIP_BIOS_GBA,
        .system_id = "GBA",
        .filename = "gba_bios.bin",
        .expected_size = 16384,
    },
};

static const flip_bios_definition_t *find_definition(
    flip_bios_kind_t kind)
{
    for (size_t i = 0;
         i < sizeof(BIOS_DEFINITIONS) / sizeof(BIOS_DEFINITIONS[0]);
         ++i) {
        if (BIOS_DEFINITIONS[i].kind == kind)
            return &BIOS_DEFINITIONS[i];
    }

    return NULL;
}

static const char *status_name(
    flip_bios_status_t status)
{
    switch (status) {
    case FLIP_BIOS_MISSING:
        return "MISSING";
    case FLIP_BIOS_VALID:
        return "VALID";
    case FLIP_BIOS_INVALID:
        return "INVALID";
    case FLIP_BIOS_ERROR:
        return "ERROR";
    }

    return "ERROR";
}

static const char *kind_name(
    flip_bios_kind_t kind)
{
    switch (kind) {
    case FLIP_BIOS_GB_DMG:
        return "GB_DMG";
    case FLIP_BIOS_GB_CGB:
        return "GB_CGB";
    case FLIP_BIOS_GBA:
        return "GBA";
    }

    return "UNKNOWN";
}

int flip_bios_system_directory(
    const char *system_id,
    char *path,
    size_t path_size)
{
    if (!system_id || !path || path_size == 0)
        return -1;

    const char *directory_name = NULL;

    if (strcmp(system_id, "GB") == 0)
        directory_name = "gb";
    else if (strcmp(system_id, "GBA") == 0)
        directory_name = "gba";
    else
        return -1;

    int written = snprintf(
        path,
        path_size,
        "%s/%s",
        flip_paths_system_root(),
        directory_name);

    if (written < 0 || (size_t)written >= path_size)
        return -1;

    return 0;
}

int flip_bios_query(
    flip_bios_kind_t kind,
    flip_bios_info_t *info)
{
    if (!info)
        return -1;

    const flip_bios_definition_t *definition =
        find_definition(kind);

    if (!definition)
        return -1;

    memset(info, 0, sizeof(*info));

    info->kind = definition->kind;
    info->status = FLIP_BIOS_ERROR;
    info->system_id = definition->system_id;
    info->filename = definition->filename;
    info->expected_size = definition->expected_size;

    char directory[FLIP_BIOS_PATH_MAX];

    if (flip_bios_system_directory(
            definition->system_id,
            directory,
            sizeof(directory)) < 0)
        goto done;

    int written = snprintf(
        info->path,
        sizeof(info->path),
        "%s/%s",
        directory,
        definition->filename);

    if (written < 0 ||
        (size_t)written >= sizeof(info->path))
        goto done;

    struct stat st;

    if (stat(info->path, &st) < 0) {
        info->status =
            errno == ENOENT
                ? FLIP_BIOS_MISSING
                : FLIP_BIOS_ERROR;
        goto done;
    }

    if (!S_ISREG(st.st_mode)) {
        info->status = FLIP_BIOS_INVALID;
        goto done;
    }

    if (st.st_size < 0) {
        info->status = FLIP_BIOS_INVALID;
        goto done;
    }

    info->actual_size = (size_t)st.st_size;
    info->status =
        info->actual_size == info->expected_size
            ? FLIP_BIOS_VALID
            : FLIP_BIOS_INVALID;

done:
    fprintf(
        stderr,
        "BIOS: kind=%s status=%s expected=%zu actual=%zu path=%s\n",
        kind_name(info->kind),
        status_name(info->status),
        info->expected_size,
        info->actual_size,
        info->path[0] ? info->path : "(unavailable)");

    return 0;
}
