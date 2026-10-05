#ifndef FLIP_LIBRETRO_CORE_H
#define FLIP_LIBRETRO_CORE_H

#include "flip_core.h"

typedef struct {
    const char *bios_system_id;
    const char *storage_id;
} flip_libretro_core_config_t;

flip_core_adapter_t *flip_libretro_core_adapter(
    const flip_libretro_core_config_t *config);

#endif
