#include "flip_session_adapter.h"
#include "flip_recovery.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/*
 * Gambatte / FlipGB process protocol:
 *
 *   exit 0  = normal exit
 *   exit 20 = saved state + suspend/GameSwitcher request
 */
static flip_session_action_t
gb_map_exit_code(
    int exit_code)
{
    if (exit_code == 20)
        return FLIP_SESSION_ACTION_SUSPEND;

    return FLIP_SESSION_ACTION_EXIT;
}

/*
 * FlipGB currently identifies automatic resume states using
 * an FNV-1a hash of the original ROM path.
 *
 * GB and GBC deliberately share this Gambatte persistence
 * mechanism for now. The ROM path keeps their states distinct.
 */
static uint64_t
gb_resume_path_hash(
    const char *text)
{
    uint64_t hash =
        UINT64_C(14695981039346656037);

    while (text &&
           *text) {

        hash ^=
            (unsigned char)*text++;

        hash *=
            UINT64_C(1099511628211);
    }

    return hash;
}

static int
gb_resume_available(
    const char *rom_path)
{
    if (!rom_path ||
        !rom_path[0])
        return 0;

    flip_recovery_context_t recovery_context;
    flip_recovery_context_init(
        &recovery_context,
        "gb",
        rom_path);

    if (flip_recovery_resume_blocked(
            &recovery_context)) {
        return 0;
    }

    char path[512];

    int written =
        snprintf(
            path,
            sizeof(path),
            "/mnt/SDCARD/flipos-dev/states/gb/%016llx.state",
            (unsigned long long)
                gb_resume_path_hash(
                    rom_path));

    if (written < 0 ||
        (size_t)written >= sizeof(path))
        return 0;

    return access(
        path,
        R_OK) == 0;
}

static int
gb_supports_save_safety(void)
{
    return 1;
}

static int
gb_build_start_normal_argv(
    const char *runner,
    const char *rom_path,
    char **argv,
    size_t argv_count)
{
    if (!runner ||
        !runner[0] ||
        !rom_path ||
        !rom_path[0] ||
        !argv ||
        argv_count < 4) {
        return -1;
    }

    argv[0] = (char *)runner;
    argv[1] = (char *)rom_path;
    argv[2] = (char *)"--no-resume";
    argv[3] = NULL;
    return 0;
}

static int
gb_build_save_safety_argv(
    const char *runner,
    const char *rom_path,
    const char *action,
    char **argv,
    size_t argv_count)
{
    if (!runner ||
        !runner[0] ||
        !rom_path ||
        !rom_path[0] ||
        !action ||
        !argv ||
        argv_count < 4) {
        return -1;
    }

    const char *flag = NULL;

    if (strcmp(action, "status") == 0)
        flag = "--save-safety-status";
    else if (strcmp(action, "restore-previous") == 0)
        flag = "--restore-previous-save";
    else if (strcmp(action, "undo") == 0)
        flag = "--undo-last-restore";
    else
        return -1;

    argv[0] = (char *)runner;
    argv[1] = (char *)rom_path;
    argv[2] = (char *)flag;
    argv[3] = NULL;
    return 0;
}

static const flip_session_adapter_t
gb_session_adapter = {
    .system = "GB",
    .map_exit_code = gb_map_exit_code,
    .resume_available = gb_resume_available,
    .supports_save_safety = gb_supports_save_safety,
    .build_start_normal_argv = gb_build_start_normal_argv,
    .build_save_safety_argv = gb_build_save_safety_argv
};

static const flip_session_adapter_t
gbc_session_adapter = {
    .system = "GBC",
    .map_exit_code = gb_map_exit_code,
    .resume_available = gb_resume_available,
    .supports_save_safety = gb_supports_save_safety,
    .build_start_normal_argv = gb_build_start_normal_argv,
    .build_save_safety_argv = gb_build_save_safety_argv
};

const flip_session_adapter_t *
flip_session_adapter_for_system(
    const char *system)
{
    if (!system)
        return NULL;

    if (strcmp(
            system,
            gb_session_adapter.system) == 0) {

        return &gb_session_adapter;
    }

    if (strcmp(
            system,
            gbc_session_adapter.system) == 0) {

        return &gbc_session_adapter;
    }

    return NULL;
}
