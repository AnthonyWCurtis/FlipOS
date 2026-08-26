#include "flip_core.h"
#include "flip_core_host.h"
#include "flip_gambatte_core.h"
#include "flip_save.h"
#include "flip_state.h"

#include <stdio.h>
#include <string.h>

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

static int load_battery_save(
    flip_core_adapter_t *core,
    void *userdata)
{
    flip_save_context_t *context = userdata;
    flip_save_load_result_t result =
        flip_save_load(core, context);

    switch (result) {
    case FLIP_SAVE_LOAD_NO_MEMORY:
    case FLIP_SAVE_LOAD_NO_SAVE:
    case FLIP_SAVE_LOAD_CURRENT:
    case FLIP_SAVE_LOAD_PREVIOUS:
        return 0;

    case FLIP_SAVE_LOAD_UNUSABLE:
    case FLIP_SAVE_LOAD_ERROR:
    default:
        return -1;
    }
}

static int commit_battery_save(
    flip_core_adapter_t *core,
    void *userdata)
{
    flip_save_context_t *context = userdata;
    return flip_save_commit(core, context);
}

int main(
    int argc,
    char **argv)
{
    if (argc < 2 || argc > 3) {
        fprintf(
            stderr,
            "Usage: %s game.gb [--no-resume]\n",
            argv[0]);
        return 1;
    }

    int skip_resume = 0;

    if (argc == 3) {
        if (strcmp(argv[2], "--no-resume") != 0) {
            fprintf(
                stderr,
                "Usage: %s game.gb [--no-resume]\n",
                argv[0]);
            return 1;
        }

        skip_resume = 1;
    }

    flip_state_context_t state_context;
    flip_save_context_t save_context;

    flip_state_context_init(
        &state_context,
        "gb",
        argv[1]);

    flip_save_context_init(
        &save_context,
        "gb",
        argv[1]);

    fprintf(
        stderr,
        "FLIPGB: rom=%s%s\n",
        state_context.rom_path,
        skip_resume ? " no-resume=1" : "");

    flip_core_adapter_t *core =
        flip_gambatte_core_adapter();

    flip_core_host_config_t config = {
        .core = core,
        .rom_path = state_context.rom_path,
        .fallback_fps = 59.7275,
        .load_save = load_battery_save,
        .commit_save = commit_battery_save,
        .save_userdata = &save_context,
        .save_state = save_resume_state,
        .restore_state = skip_resume ? NULL : load_resume_state,
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
