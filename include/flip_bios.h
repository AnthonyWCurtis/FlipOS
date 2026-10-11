#ifndef FLIP_BIOS_H
#define FLIP_BIOS_H

#include <stddef.h>

#define FLIP_BIOS_PATH_MAX 512
#define FLIP_BIOS_SHA256_HEX 65

typedef enum {
    FLIP_BIOS_GB_DMG = 0,
    FLIP_BIOS_GB_CGB,
    FLIP_BIOS_GBA
} flip_bios_kind_t;

typedef enum {
    FLIP_BIOS_MISSING = 0,
    FLIP_BIOS_VALID,
    FLIP_BIOS_INVALID,
    FLIP_BIOS_ERROR
} flip_bios_status_t;

typedef struct {
    flip_bios_kind_t kind;
    flip_bios_status_t status;
    const char *system_id;
    const char *filename;
    size_t expected_size;
    size_t actual_size;
    const char *expected_sha256;
    char actual_sha256[FLIP_BIOS_SHA256_HEX];
    char path[FLIP_BIOS_PATH_MAX];
} flip_bios_info_t;

int flip_bios_system_directory(
    const char *system_id,
    char *path,
    size_t path_size);

int flip_bios_query(
    flip_bios_kind_t kind,
    flip_bios_info_t *info);

#endif
