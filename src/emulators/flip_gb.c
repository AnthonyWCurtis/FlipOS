#include "flip_core.h"
#include "flip_core_host.h"
#include "flip_gambatte_core.h"
#include "flip_recovery.h"
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

static int persistence_complete(void *userdata)
{
    flip_recovery_context_t *context = userdata;

    if (flip_recovery_clear_resume_block(context) < 0) {
        fprintf(stderr, "FLIP_RECOVERY: FAILED stage=clear-marker\n");
        return -1;
    }

    return 0;
}

static const char *save_health_name(flip_save_health_t health)
{
    switch (health) {
    case FLIP_SAVE_HEALTH_HEALTHY:
        return "HEALTHY";
    case FLIP_SAVE_HEALTH_MISSING:
        return "MISSING";
    case FLIP_SAVE_HEALTH_INVALID:
    case FLIP_SAVE_HEALTH_ERROR:
    default:
        return "INVALID";
    }
}

static const char *state_recovery_health_name(
    flip_state_recovery_health_t health)
{
    switch (health) {
    case FLIP_STATE_RECOVERY_AVAILABLE:
        return "AVAILABLE";
    case FLIP_STATE_RECOVERY_MISSING:
        return "MISSING";
    case FLIP_STATE_RECOVERY_INVALID:
    case FLIP_STATE_RECOVERY_ERROR:
    default:
        return "INVALID";
    }
}

static int run_save_safety_management(
    const char *rom_path,
    const char *action)
{
    flip_state_context_t state_context;
    flip_save_context_t save_context;
    flip_recovery_context_t recovery_context;

    flip_state_context_init(&state_context, "gb", rom_path);
    flip_save_context_init(&save_context, "gb", rom_path);
    flip_recovery_context_init(&recovery_context, "gb", rom_path);

    flip_core_adapter_t *core = flip_gambatte_core_adapter();
    int initialized = 0;
    int loaded = 0;
    int result = 0;

    if (flip_core_init(core) < 0) {
        fprintf(stderr, "FLIP_SAVE_SAFETY: core-init-failed\n");
        return 1;
    }
    initialized = 1;

    if (flip_core_load_game(core, rom_path) < 0) {
        fprintf(stderr, "FLIP_SAVE_SAFETY: game-load-failed\n");
        result = 1;
        goto cleanup;
    }
    loaded = 1;

    if (strcmp(action, "--restore-previous-save") == 0) {
        flip_recovery_result_t recovery_result =
            flip_recovery_restore_previous(
                core,
                &save_context,
                &state_context,
                &recovery_context);

        if (recovery_result == FLIP_RECOVERY_NO_PREVIOUS)
            result = 2;
        else if (recovery_result != FLIP_RECOVERY_OK)
            result = 1;
    } else if (strcmp(action, "--undo-last-restore") == 0) {
        flip_recovery_result_t recovery_result =
            flip_recovery_undo(
                core,
                &save_context,
                &state_context,
                &recovery_context);

        if (recovery_result == FLIP_RECOVERY_NO_UNDO)
            result = 2;
        else if (recovery_result != FLIP_RECOVERY_OK)
            result = 1;
    }

    flip_save_status_t save_status;

    if (flip_save_query_status(
            core,
            &save_context,
            &save_status) < 0) {
        result = 1;
        goto cleanup;
    }

    flip_state_status_t state_status;

    if (flip_state_query_status(
            core,
            &state_context,
            &state_status) < 0) {
        result = 1;
        goto cleanup;
    }

    int resume_blocked =
        flip_recovery_resume_blocked(&recovery_context);

    fprintf(
        stdout,
        "FLIP_SAVE_SAFETY: current=%s previous=%s recovery=%s resume=%s undo_state=%s\n",
        save_health_name(save_status.current),
        save_health_name(save_status.previous),
        save_health_name(save_status.recovery),
        resume_blocked ? "BLOCKED" : "AVAILABLE",
        state_recovery_health_name(state_status.recovery));

    if (save_status.current == FLIP_SAVE_HEALTH_ERROR ||
        save_status.previous == FLIP_SAVE_HEALTH_ERROR ||
        save_status.recovery == FLIP_SAVE_HEALTH_ERROR ||
        state_status.recovery == FLIP_STATE_HEALTH_ERROR) {
        result = 1;
    }

cleanup:
    if (loaded)
        flip_core_unload_game(core);
    if (initialized)
        flip_core_deinit(core);

    return result;
}

static void print_usage(const char *program)
{
    fprintf(
        stderr,
        "Usage: %s game.gb [--no-resume|--save-safety-status|--restore-previous-save|--undo-last-restore]\n",
        program);
}

int main(
    int argc,
    char **argv)
{
    if (argc < 2 || argc > 3) {
        print_usage(argv[0]);
        return 1;
    }

    if (argc == 3 &&
        (strcmp(argv[2], "--save-safety-status") == 0 ||
         strcmp(argv[2], "--restore-previous-save") == 0 ||
         strcmp(argv[2], "--undo-last-restore") == 0)) {
        return run_save_safety_management(argv[1], argv[2]);
    }

    int skip_resume = 0;

    if (argc == 3) {
        if (strcmp(argv[2], "--no-resume") != 0) {
            print_usage(argv[0]);
            return 1;
        }

        skip_resume = 1;
    }

    flip_state_context_t state_context;
    flip_save_context_t save_context;
    flip_recovery_context_t recovery_context;

    flip_state_context_init(
        &state_context,
        "gb",
        argv[1]);

    flip_save_context_init(
        &save_context,
        "gb",
        argv[1]);

    flip_recovery_context_init(
        &recovery_context,
        "gb",
        argv[1]);

    int recovery_blocked =
        flip_recovery_resume_blocked(
            &recovery_context);

    if (recovery_blocked)
        fprintf(stderr, "FLIP_RECOVERY: RESUME_BLOCKED\n");

    fprintf(
        stderr,
        "FLIPGB: rom=%s%s%s\n",
        state_context.rom_path,
        skip_resume ? " no-resume=1" : "",
        recovery_blocked ? " recovery-blocked=1" : "");

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
        .restore_state = (skip_resume || recovery_blocked)
            ? NULL
            : load_resume_state,
        .state_userdata = &state_context,
        .persistence_complete = persistence_complete,
        .persistence_userdata = &recovery_context,
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
