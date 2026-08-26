#include "flip_core.h"
#include "flip_core_host.h"
#include "flip_gambatte_core.h"
#include "flip_state.h"

#include <stdio.h>

static int save_resume_state(
    flip_core_adapter_t *core,
    void *userdata)
{
    flip_state_context_t *context = userdata;

    return flip_state_save(
        core,
        context);
}

static int load_resume_state(
    flip_core_adapter_t *core,
    void *userdata)
{
    flip_state_context_t *context = userdata;

    flip_state_restore_result_t result =
        flip_state_restore(
            core,
            context);

    switch (result) {
    case FLIP_STATE_RESTORE_CURRENT:
    case FLIP_STATE_RESTORE_PREVIOUS:
        return 1;

    case FLIP_STATE_RESTORE_NONE:
        return 0;

    case FLIP_STATE_RESTORE_UNUSABLE:
    case FLIP_STATE_RESTORE_ERROR:
    default:
        return -1;
    }
}

int main(
    int argc,
    char **argv)
{
    if (argc < 2) {
        fprintf(
            stderr,
            "Usage: %s game.gb\n",
            argv[0]);
        return 1;
    }

    flip_state_context_t state_context;

    flip_state_context_init(
        &state_context,
        "gb",
        argv[1]);

    fprintf(
        stderr,
        "FLIPGB: rom=%s\n",
        state_context.rom_path);

    flip_core_adapter_t *core =
        flip_gambatte_core_adapter();

    flip_core_host_config_t config = {
        .core = core,
        .rom_path = state_context.rom_path,
        .fallback_fps = 59.7275,
        .save_state = save_resume_state,
        .restore_state = load_resume_state,
        .state_userdata = &state_context,
    };

    int exit_code = 0;

    if (flip_core_host_run(
            &config,
            &exit_code) < 0) {

        fprintf(
            stderr,
            "FLIPGB: core host failed\n");
        return 1;
    }

    fprintf(
        stderr,
        "FLIPGB: clean exit\n");

    return exit_code;
}
