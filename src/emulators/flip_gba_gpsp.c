#include "flip_emulator_runner.h"
#include "flip_gpsp_core.h"

/*
 * Experimental native GBA runner. Never replace the installed flip-gba
 * binary or share mGBA saves/states without a reviewed migration.
 */
int main(int argc, char **argv)
{
    flip_emulator_runner_config_t config = {
        .storage_id = "gba-gpsp-test",
        .log_name = "FLIPGBA_GPSP_TEST",
        .fallback_fps = 59.7275,
        .core = flip_gpsp_core_adapter(),
    };

    return flip_emulator_run(&config, argc, argv);
}
