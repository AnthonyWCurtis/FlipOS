#define _POSIX_C_SOURCE 200809L

#include "flip_save_safety_ui.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/*
 * Save Safety restore/undo is intentionally hidden for Beta until its
 * user-facing recovery workflow has completed release hardening.
 * Internal save rotation remains active.
 */
#define FLIP_SAVE_SAFETY_USER_ENABLED 0

int flip_save_safety_user_enabled(void)
{
    return FLIP_SAVE_SAFETY_USER_ENABLED;
}

static flip_save_safety_health_t parse_health(const char *text)
{
    if (!text)
        return FLIP_SAVE_SAFETY_HEALTH_UNKNOWN;
    if (strcmp(text, "HEALTHY") == 0 || strcmp(text, "AVAILABLE") == 0)
        return FLIP_SAVE_SAFETY_HEALTH_HEALTHY;
    if (strcmp(text, "MISSING") == 0)
        return FLIP_SAVE_SAFETY_HEALTH_MISSING;
    if (strcmp(text, "INVALID") == 0)
        return FLIP_SAVE_SAFETY_HEALTH_INVALID;
    return FLIP_SAVE_SAFETY_HEALTH_UNKNOWN;
}

const char *flip_save_safety_health_name(
    flip_save_safety_health_t health)
{
    switch (health) {
    case FLIP_SAVE_SAFETY_HEALTH_HEALTHY:
        return "HEALTHY";
    case FLIP_SAVE_SAFETY_HEALTH_MISSING:
        return "MISSING";
    case FLIP_SAVE_SAFETY_HEALTH_INVALID:
        return "INVALID";
    case FLIP_SAVE_SAFETY_HEALTH_UNKNOWN:
    default:
        return "UNKNOWN";
    }
}

int flip_save_safety_supported(const flip_system_t *system)
{
    return system &&
        system->session_adapter &&
        system->session_adapter->supports_save_safety &&
        system->session_adapter->supports_save_safety();
}

int flip_save_safety_build_start_normal(
    const flip_system_t *system,
    const char *rom_path,
    char **argv,
    size_t argv_count)
{
    if (!system ||
        !system->runner ||
        !system->session_adapter ||
        !system->session_adapter->build_start_normal_argv) {
        return -1;
    }

    return system->session_adapter->build_start_normal_argv(
        system->runner,
        rom_path,
        argv,
        argv_count);
}

static int run_management_command(
    const flip_system_t *system,
    const char *rom_path,
    const char *action,
    char *output,
    size_t output_size)
{
    if (!flip_save_safety_user_enabled() ||
        !flip_save_safety_supported(system) ||
        !rom_path ||
        !rom_path[0] ||
        !action ||
        !output ||
        output_size < 2 ||
        !system->session_adapter->build_save_safety_argv) {
        return -1;
    }

    char *argv[5] = {0};
    if (system->session_adapter->build_save_safety_argv(
            system->runner,
            rom_path,
            action,
            argv,
            sizeof(argv) / sizeof(argv[0])) < 0) {
        return -1;
    }

    int pipefd[2];
    if (pipe(pipefd) < 0)
        return -1;

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }

    if (pid == 0) {
        close(pipefd[0]);
        if (dup2(pipefd[1], STDOUT_FILENO) < 0)
            _exit(126);
        close(pipefd[1]);
        execv(system->runner, argv);
        _exit(127);
    }

    close(pipefd[1]);

    size_t used = 0;
    while (used + 1 < output_size) {
        ssize_t got = read(pipefd[0], output + used, output_size - used - 1);
        if (got > 0) {
            used += (size_t)got;
            continue;
        }
        if (got < 0 && errno == EINTR)
            continue;
        break;
    }
    output[used] = '\0';
    close(pipefd[0]);

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR)
            return -1;
    }

    if (!WIFEXITED(status))
        return -1;

    return WEXITSTATUS(status);
}

int flip_save_safety_query(
    const flip_system_t *system,
    const char *rom_path,
    flip_save_safety_status_t *status)
{
    if (!status)
        return -1;

    memset(status, 0, sizeof(*status));

    char output[512];
    int exit_code = run_management_command(
        system,
        rom_path,
        "status",
        output,
        sizeof(output));

    if (exit_code != 0)
        return -1;

    const char *prefix = "FLIP_SAVE_SAFETY:";
    char *line = strstr(output, prefix);
    if (!line)
        return -1;

    char current[16];
    char previous[16];
    char recovery[16];
    char resume[16];
    char undo_state[16];

    int fields = sscanf(
        line,
        "FLIP_SAVE_SAFETY: current=%15s previous=%15s recovery=%15s resume=%15s undo_state=%15s",
        current,
        previous,
        recovery,
        resume,
        undo_state);

    if (fields != 5)
        return -1;

    status->current = parse_health(current);
    status->previous = parse_health(previous);
    status->recovery = parse_health(recovery);
    status->resume_blocked = strcmp(resume, "BLOCKED") == 0;
    status->undo_state = parse_health(undo_state);

    if (status->current == FLIP_SAVE_SAFETY_HEALTH_UNKNOWN ||
        status->previous == FLIP_SAVE_SAFETY_HEALTH_UNKNOWN ||
        status->recovery == FLIP_SAVE_SAFETY_HEALTH_UNKNOWN ||
        status->undo_state == FLIP_SAVE_SAFETY_HEALTH_UNKNOWN ||
        (strcmp(resume, "BLOCKED") != 0 && strcmp(resume, "AVAILABLE") != 0)) {
        return -1;
    }

    return 0;
}

static int run_mutation(
    const flip_system_t *system,
    const char *rom_path,
    const char *action)
{
    char output[512];
    int exit_code = run_management_command(
        system,
        rom_path,
        action,
        output,
        sizeof(output));

    if (exit_code == 0)
        return 0;
    if (exit_code == 2)
        return 1;
    return -1;
}

int flip_save_safety_restore_previous(
    const flip_system_t *system,
    const char *rom_path)
{
    return run_mutation(system, rom_path, "restore-previous");
}

int flip_save_safety_undo(
    const flip_system_t *system,
    const char *rom_path)
{
    return run_mutation(system, rom_path, "undo");
}
