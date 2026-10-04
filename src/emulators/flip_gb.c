#include "flip_emulator_runner.h"
#include "flip_gambatte_core.h"

int main(int argc, char **argv)
{
    flip_emulator_runner_config_t config = {
        .storage_id = "gb",
        .log_name = "FLIPGB",
        .fallback_fps = 59.7275,
        .core = flip_gambatte_core_adapter(),
    };

    return flip_emulator_run(&config, argc, argv);
}
