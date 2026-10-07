#define _DEFAULT_SOURCE
#include "flip_optimizer_quiesce.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int contains_component(
    const char *cmdline,
    const char *needle)
{
    return cmdline &&
        needle &&
        strstr(cmdline, needle) != NULL;
}

int flip_optimizer_quiesce_cmdline_is_writer(
    const char *cmdline)
{
    if (!cmdline || !cmdline[0])
        return 0;

    if (contains_component(
            cmdline,
            "flip-optimizer-"))
        return 0;

    if (contains_component(
            cmdline,
            "/flip-frontend") ||
        contains_component(
            cmdline,
            "/flip-gb") ||
        contains_component(
            cmdline,
            "/flip-gba") ||
        contains_component(
            cmdline,
            "/flip-gpu-service"))
        return 1;

    if (contains_component(
            cmdline,
            "/flip-perf-capture") &&
        contains_component(
            cmdline,
            "playtest"))
        return 1;

    return 0;
}

static int read_cmdline(
    pid_t pid,
    char *buffer,
    size_t size)
{
    if (!buffer || size < 2)
        return -1;

    char path[64];
    int n = snprintf(
        path,
        sizeof(path),
        "/proc/%ld/cmdline",
        (long)pid);
    if (n < 0 || (size_t)n >= sizeof(path))
        return -1;

    FILE *file = fopen(path, "r");
    if (!file)
        return errno == ENOENT
            ? 0
            : -1;

    size_t used =
        fread(buffer, 1, size - 1, file);
    int read_failed = ferror(file);
    int close_failed = fclose(file) != 0;

    if (read_failed || close_failed)
        return -1;

    if (used == 0)
        return 0;

    buffer[used] = '\0';

    for (size_t i = 0; i < used; ++i) {
        if (buffer[i] == '\0')
            buffer[i] = ' ';
    }

    return 1;
}

static int pid_is_stopped(pid_t pid)
{
    char path[64];
    int n = snprintf(
        path,
        sizeof(path),
        "/proc/%ld/status",
        (long)pid);
    if (n < 0 || (size_t)n >= sizeof(path))
        return -1;

    FILE *file = fopen(path, "r");
    if (!file)
        return errno == ENOENT
            ? 1
            : -1;

    char line[256];
    int stopped = 0;

    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "State:", 6) != 0)
            continue;

        const char *p = line + 6;
        while (*p && isspace((unsigned char)*p))
            ++p;

        stopped =
            *p == 'T' ||
            *p == 't';
        break;
    }

    if (fclose(file) != 0)
        return -1;

    return stopped;
}

static void resume_recorded(
    flip_optimizer_quiesce_t *state)
{
    if (!state)
        return;

    for (size_t i = 0; i < state->count; ++i) {
        if (state->pids[i] > 1)
            (void)kill(state->pids[i], SIGCONT);
    }

    state->count = 0;
    state->active = 0;
}

int flip_optimizer_quiesce_begin(
    flip_optimizer_quiesce_t *state)
{
    if (!state || state->active)
        return -1;

    memset(state, 0, sizeof(*state));

    DIR *proc = opendir("/proc");
    if (!proc)
        return -1;

    pid_t self = getpid();
    struct dirent *entry = NULL;
    int failed = 0;

    while ((entry = readdir(proc)) != NULL) {
        char *end = NULL;
        errno = 0;
        long parsed =
            strtol(entry->d_name, &end, 10);

        if (errno ||
            end == entry->d_name ||
            *end != '\0' ||
            parsed <= 1 ||
            parsed > 0x7fffffffL)
            continue;

        pid_t pid = (pid_t)parsed;
        if (pid == self)
            continue;

        char cmdline[1024];
        int rc = read_cmdline(
            pid,
            cmdline,
            sizeof(cmdline));

        if (rc < 0) {
            failed = 1;
            break;
        }

        if (rc == 0 ||
            !flip_optimizer_quiesce_cmdline_is_writer(
                cmdline))
            continue;

        if (state->count >=
            FLIP_OPTIMIZER_QUIESCE_MAX_PIDS) {
            failed = 1;
            break;
        }

        if (kill(pid, SIGSTOP) < 0) {
            if (errno == ESRCH)
                continue;
            failed = 1;
            break;
        }

        state->pids[state->count++] = pid;
    }

    int saved_errno = errno;
    closedir(proc);
    errno = saved_errno;

    if (failed) {
        resume_recorded(state);
        return -1;
    }

    for (unsigned attempt = 0;
         attempt < 100;
         ++attempt) {
        size_t stopped = 0;

        for (size_t i = 0;
             i < state->count;
             ++i) {
            int rc =
                pid_is_stopped(state->pids[i]);

            if (rc < 0) {
                resume_recorded(state);
                return -1;
            }

            if (rc == 1)
                ++stopped;
        }

        if (stopped == state->count)
            break;

        if (attempt == 99) {
            resume_recorded(state);
            return -1;
        }

        const struct timespec delay = {
            .tv_sec = 0,
            .tv_nsec = 10000000L,
        };
        (void)nanosleep(&delay, NULL);
    }

    /*
     * The one deliberate storage flush happens while CPU voltage is still
     * stock/parked. No FlipOS writer should run again until quiesce_end().
     */
    sync();

    state->active = 1;
    return 0;
}

int flip_optimizer_quiesce_end(
    flip_optimizer_quiesce_t *state)
{
    if (!state)
        return -1;

    if (!state->active &&
        state->count == 0)
        return 0;

    int failed = 0;

    for (size_t i = 0;
         i < state->count;
         ++i) {
        if (state->pids[i] <= 1)
            continue;

        if (kill(
                state->pids[i],
                SIGCONT) < 0 &&
            errno != ESRCH)
            failed = 1;
    }

    state->count = 0;
    state->active = 0;
    return failed ? -1 : 0;
}
