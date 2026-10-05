#include "flip_gambatte_core.h"

#include "flip_libretro_core.h"

flip_core_adapter_t *flip_gambatte_core_adapter(void)
{
    static const flip_libretro_core_config_t config = {
        .bios_system_id = "GB",
        .storage_id = "gb",
    };

    return flip_libretro_core_adapter(&config);
}
