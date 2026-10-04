#include "flip_emulator_runner.h"
#include "flip_mgba_core.h"

int main(int argc, char **argv)
{
    flip_emulator_runner_config_t config = {
        .storage_id = "gba",
        .log_name = "FLIPGBA",
        .fallback_fps = 59.7275,
        .core = flip_mgba_core_adapter(),
    };

    return flip_emulator_run(&config, argc, argv);
}
