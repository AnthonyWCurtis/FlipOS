#include "flip_gpsp_core.h"

#include "flip_libretro_core.h"

flip_core_adapter_t *flip_gpsp_core_adapter(void)
{
    static const flip_libretro_core_config_t config = {
        .bios_system_id = "GBA",
        .storage_id = "gba-gpsp-test",
    };

    return flip_libretro_core_adapter(&config);
}
