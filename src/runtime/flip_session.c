#define _GNU_SOURCE

#include "flip_session.h"
#include "flip_session_adapter.h"
#include "flip_system.h"
#include "flip_input.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifndef FLIP_SESSION_STATE_ROOT
#define FLIP_SESSION_STATE_ROOT \
    "/mnt/SDCARD/flipos-dev/state"
#endif

#define SESSION_DIR \
    FLIP_SESSION_STATE_ROOT "/session"

#define SESSION_FILE \
    FLIP_SESSION_STATE_ROOT "/session/current"

#define RECENT_DIR \
    "/mnt/SDCARD/flipos-dev/state/recent"

#define RECENT_FILE \
    "/mnt/SDCARD/flipos-dev/state/recent/history"

#define RECENT_MAX 10

flip_session_action_t
flip_session_action_from_exit_code(
    int exit_code)
{
    if (exit_code ==
        FLIP_SESSION_EXIT_SAVED_HANDOFF)
        return FLIP_SESSION_ACTION_SUSPEND;

    return FLIP_SESSION_ACTION_EXIT;
}

static long long now_epoch(void)
{
    return (long long)time(NULL);
}

static int copy_string(
    char *dest,
    size_t dest_size,
    const char *source)
{
    if (!dest ||
        dest_size == 0)
        return -1;

    if (!source) {
        dest[0] = '\0';
        return 0;
    }

    size_t length =
        strlen(source);

    if (length >= dest_size) {
        memcpy(
            dest,
            source,
            dest_size - 1);

        dest[dest_size - 1] =
            '\0';

        return 1;
    }

    memcpy(
        dest,
        source,
        length + 1);

    return 0;
}

static int ensure_session_dir(void)
{
    if (mkdir(
            FLIP_SESSION_STATE_ROOT,
            0777) < 0 &&
        errno != EEXIST)
        return -1;

    if (mkdir(
            SESSION_DIR,
            0777) < 0 &&
        errno != EEXIST)
        return -1;

    return 0;
}

static int parse_int_value(
    const char *text,
    int *value)
{
    if (!text ||
        !text[0] ||
        !value)
        return -1;

    errno = 0;

    char *end = NULL;

    long parsed =
        strtol(
            text,
            &end,
            10);

    if (errno != 0 ||
        !end ||
        *end != '\0' ||
        parsed < INT_MIN ||
        parsed > INT_MAX)
        return -1;

    *value = (int)parsed;

    return 0;
}

static int parse_ll_value(
    const char *text,
    long long *value)
{
    if (!text ||
        !text[0] ||
        !value)
        return -1;

    errno = 0;

    char *end = NULL;

    long long parsed =
        strtoll(
            text,
            &end,
            10);

    if (errno != 0 ||
        !end ||
        *end != '\0')
        return -1;

    *value = parsed;

    return 0;
}

static int write_session_state(
    const char *state,
    const char *program,
    const flip_session_metadata_t *metadata,
    pid_t pid,
    int exit_code,
    int signal_number,
    long long started,
    long long ended)
{
    if (ensure_session_dir() < 0) {
        fprintf(
            stderr,
            "SESSION_STATE: mkdir failed: %s\n",
            strerror(errno));

        return -1;
    }

    char tmp_path[512];

    snprintf(
        tmp_path,
        sizeof(tmp_path),
        "%s.tmp",
        SESSION_FILE);

    FILE *f =
        fopen(
            tmp_path,
            "w");

    if (!f) {
        fprintf(
            stderr,
            "SESSION_STATE: fopen failed: %s\n",
            strerror(errno));

        return -1;
    }

    fprintf(
        f,
        "version=1\n");

    fprintf(
        f,
        "state=%s\n",
        state ? state : "unknown");

    fprintf(
        f,
        "system=%s\n",
        metadata &&
        metadata->system &&
        metadata->system->id
            ? metadata->system->id
            : "");

    fprintf(
        f,
        "rom=%s\n",
        metadata &&
        metadata->rom_path
            ? metadata->rom_path
            : "");

    fprintf(
        f,
        "program=%s\n",
        program ? program : "");

    fprintf(
        f,
        "pid=%d\n",
        (int)pid);

    fprintf(
        f,
        "started=%lld\n",
        started);

    fprintf(
        f,
        "ended=%lld\n",
        ended);

    fprintf(
        f,
        "exit_code=%d\n",
        exit_code);

    fprintf(
        f,
        "signal=%d\n",
        signal_number);

    fflush(f);
    fsync(fileno(f));
    fclose(f);

    if (rename(
            tmp_path,
            SESSION_FILE) < 0) {

        fprintf(
            stderr,
            "SESSION_STATE: rename failed: %s\n",
            strerror(errno));

        unlink(tmp_path);

        return -1;
    }

    fprintf(
        stderr,
        "SESSION_STATE: state=%s rom=%s\n",
        state ? state : "unknown",
        metadata &&
        metadata->rom_path
            ? metadata->rom_path
            : "");

    return 0;
}

static int update_recent_history(
    const flip_session_metadata_t *metadata)
{
    if (!metadata ||
        !metadata->system ||
        !metadata->system->id ||
        !metadata->rom_path ||
        !metadata->system->id[0] ||
        !metadata->rom_path[0])
        return 0;

    if (mkdir(
            RECENT_DIR,
            0777) < 0 &&
        errno != EEXIST) {

        fprintf(
            stderr,
            "SESSION_RECENT: mkdir failed: %s\n",
            strerror(errno));

        return -1;
    }

    typedef struct {
        char system[32];
        char rom[512];
    } recent_entry_t;

    recent_entry_t entries[RECENT_MAX];
    int count = 0;

    FILE *f =
        fopen(
            RECENT_FILE,
            "r");

    if (f) {
        char line[640];

        while (fgets(
                   line,
                   sizeof(line),
                   f) &&
               count < RECENT_MAX) {

            char *newline =
                strchr(line, '\n');

            if (newline)
                *newline = '\0';

            char *sep =
                strchr(line, '|');

            if (!sep)
                continue;

            *sep = '\0';

            const char *system =
                line;

            const char *rom =
                sep + 1;

            if (!system[0] ||
                !rom[0])
                continue;

            /*
             * Skip the game being relaunched so it can
             * be moved back to the top.
             */
            if (strcmp(
                    rom,
                    metadata->rom_path) == 0)
                continue;

            copy_string(
                entries[count].system,
                sizeof(entries[count].system),
                system);

            snprintf(
                entries[count].rom,
                sizeof(entries[count].rom),
                "%s",
                rom);

            count++;
        }

        fclose(f);
    }

    char tmp_path[512];

    snprintf(
        tmp_path,
        sizeof(tmp_path),
        "%s.tmp",
        RECENT_FILE);

    f =
        fopen(
            tmp_path,
            "w");

    if (!f) {
        fprintf(
            stderr,
            "SESSION_RECENT: fopen failed: %s\n",
            strerror(errno));

        return -1;
    }

    fprintf(
        f,
        "%s|%s\n",
        metadata->system->id,
        metadata->rom_path);

    for (int i = 0;
         i < count &&
         i < RECENT_MAX - 1;
         ++i) {

        fprintf(
            f,
            "%s|%s\n",
            entries[i].system,
            entries[i].rom);
    }

    fflush(f);
    fsync(fileno(f));
    fclose(f);

    if (rename(
            tmp_path,
            RECENT_FILE) < 0) {

        fprintf(
            stderr,
            "SESSION_RECENT: rename failed: %s\n",
            strerror(errno));

        unlink(tmp_path);

        return -1;
    }

    fprintf(
        stderr,
        "SESSION_RECENT: updated rom=%s\n",
        metadata->rom_path);

    return 0;
}

int flip_session_remove_recent(
    const flip_system_t *system,
    const char *rom_path)
{
    if (!system ||
        !system->id ||
        !rom_path ||
        !rom_path[0])
        return -1;

    typedef struct {
        char system[32];
        char rom[512];
    } recent_entry_t;

    recent_entry_t entries[RECENT_MAX];
    int count = 0;
    int removed = 0;

    FILE *f =
        fopen(
            RECENT_FILE,
            "r");

    if (!f)
        return 0;

    char line[640];

    while (fgets(
               line,
               sizeof(line),
               f) &&
           count < RECENT_MAX) {

        char *newline =
            strchr(line, '\n');

        if (newline)
            *newline = '\0';

        char *sep =
            strchr(line, '|');

        if (!sep)
            continue;

        *sep = '\0';

        const char *entry_system =
            line;

        const char *entry_rom =
            sep + 1;

        if (!entry_system[0] ||
            !entry_rom[0])
            continue;

        if (strcmp(
                entry_system,
                system->id) == 0 &&
            strcmp(
                entry_rom,
                rom_path) == 0) {

            removed = 1;
            continue;
        }

        copy_string(
            entries[count].system,
            sizeof(entries[count].system),
            entry_system);

        snprintf(
            entries[count].rom,
            sizeof(entries[count].rom),
            "%s",
            entry_rom);

        count++;
    }

    fclose(f);

    if (!removed)
        return 0;

    char tmp_path[512];

    snprintf(
        tmp_path,
        sizeof(tmp_path),
        "%s.tmp",
        RECENT_FILE);

    f =
        fopen(
            tmp_path,
            "w");

    if (!f) {

        fprintf(
            stderr,
            "SESSION_RECENT: remove fopen failed: %s\n",
            strerror(errno));

        return -1;
    }

    for (int i = 0;
         i < count;
         ++i) {

        fprintf(
            f,
            "%s|%s\n",
            entries[i].system,
            entries[i].rom);
    }

    fflush(f);
    fsync(fileno(f));
    fclose(f);

    if (rename(
            tmp_path,
            RECENT_FILE) < 0) {

        fprintf(
            stderr,
            "SESSION_RECENT: remove rename failed: %s\n",
            strerror(errno));

        unlink(tmp_path);

        return -1;
    }

    fprintf(
        stderr,
        "SESSION_RECENT: removed rom=%s\n",
        rom_path);

    return 1;
}

int flip_session_prune_recent(void)
{
    typedef struct {
        char system[32];
        char rom[512];
    } recent_entry_t;

    recent_entry_t entries[RECENT_MAX];

    int count = 0;
    int stale = 0;
    int malformed = 0;
    int duplicate = 0;

    FILE *f =
        fopen(
            RECENT_FILE,
            "r");

    if (!f)
        return 0;

    char line[640];

    while (fgets(
               line,
               sizeof(line),
               f)) {

        char *newline =
            strchr(
                line,
                '\n');

        if (newline)
            *newline = '\0';

        char *sep =
            strchr(
                line,
                '|');

        if (!sep) {
            malformed++;
            continue;
        }

        *sep = '\0';

        const char *system_id =
            line;

        const char *rom_path =
            sep + 1;

        if (!system_id[0] ||
            !rom_path[0]) {

            malformed++;
            continue;
        }

        const flip_system_t *system =
            flip_system_find(
                system_id);

        if (!system ||
            access(
                rom_path,
                R_OK) != 0 ||
            !flip_system_supports_path(
                system,
                rom_path)) {

            stale++;
            continue;
        }

        int already_present = 0;

        for (int i = 0;
             i < count;
             ++i) {

            if (strcmp(
                    entries[i].system,
                    system_id) == 0 &&
                strcmp(
                    entries[i].rom,
                    rom_path) == 0) {

                already_present = 1;
                break;
            }
        }

        if (already_present) {
            duplicate++;
            continue;
        }

        if (count >= RECENT_MAX) {
            stale++;
            continue;
        }

        copy_string(
            entries[count].system,
            sizeof(entries[count].system),
            system_id);

        copy_string(
            entries[count].rom,
            sizeof(entries[count].rom),
            rom_path);

        count++;
    }

    fclose(f);

    int removed =
        stale +
        malformed +
        duplicate;

    if (removed <= 0)
        return 0;

    char tmp_path[512];

    snprintf(
        tmp_path,
        sizeof(tmp_path),
        "%s.tmp",
        RECENT_FILE);

    f =
        fopen(
            tmp_path,
            "w");

    if (!f) {

        fprintf(
            stderr,
            "SESSION_RECENT: prune fopen failed: %s\n",
            strerror(errno));

        return -1;
    }

    for (int i = 0;
         i < count;
         ++i) {

        fprintf(
            f,
            "%s|%s\n",
            entries[i].system,
            entries[i].rom);
    }

    fflush(f);
    fsync(fileno(f));
    fclose(f);

    if (rename(
            tmp_path,
            RECENT_FILE) < 0) {

        fprintf(
            stderr,
            "SESSION_RECENT: prune rename failed: %s\n",
            strerror(errno));

        unlink(tmp_path);

        return -1;
    }

    fprintf(
        stderr,
        "SESSION_RECENT: pruned valid=%d stale=%d malformed=%d duplicate=%d\n",
        count,
        stale,
        malformed,
        duplicate);

    return removed;
}

static int restore_hardware(
    flip_drm_t *drm)
{
    if (flip_drm_open(drm) < 0) {
        fprintf(
            stderr,
            "SESSION: failed to reacquire DRM\n");

        return -1;
    }

    if (flip_input_open() < 0) {
        fprintf(
            stderr,
            "SESSION: failed to reacquire input\n");

        flip_drm_close(drm);

        return -1;
    }

    fprintf(
        stderr,
        "SESSION: hardware restored\n");

    return 0;
}

int flip_session_launch(
    flip_drm_t *drm,
    const char *program,
    char *const argv[],
    const flip_session_metadata_t *metadata,
    flip_session_result_t *result)
{
    if (result)
        memset(result, 0, sizeof(*result));

    if (!drm ||
        !program ||
        !argv)
        return -1;

    flip_session_result_t local;

    memset(
        &local,
        0,
        sizeof(local));

    if (!result)
        result = &local;

    memset(
        result,
        0,
        sizeof(*result));

    fprintf(
        stderr,
        "SESSION: launch program=%s\n",
        program);

    flip_input_close();
    flip_drm_close(drm);

    long long started =
        now_epoch();

    pid_t pid =
        fork();

    if (pid < 0) {

        fprintf(
            stderr,
            "SESSION: fork failed: %s\n",
            strerror(errno));

        restore_hardware(drm);

        return -1;
    }

    if (pid == 0) {

        execv(
            program,
            argv);

        fprintf(
            stderr,
            "SESSION: exec failed program=%s error=%s\n",
            program,
            strerror(errno));

        _exit(127);
    }

    result->launched = 1;

    update_recent_history(
        metadata);

    write_session_state(
        "running",
        program,
        metadata,
        pid,
        0,
        0,
        started,
        0);

    fprintf(
        stderr,
        "SESSION: child pid=%d\n",
        (int)pid);

    int status = 0;

    while (waitpid(
               pid,
               &status,
               0) < 0) {

        if (errno == EINTR)
            continue;

        fprintf(
            stderr,
            "SESSION: waitpid failed: %s\n",
            strerror(errno));

        write_session_state(
            "error",
            program,
            metadata,
            pid,
            -1,
            0,
            started,
            now_epoch());

        if (restore_hardware(drm) < 0)
            return -1;

        return -1;
    }

    result->exited = 1;

    int exit_code = 0;
    int signal_number = 0;

    if (WIFEXITED(status)) {

        exit_code =
            WEXITSTATUS(status);

        result->exit_code =
            exit_code;

        result->action =
            flip_session_action_from_exit_code(
                exit_code);

        fprintf(
            stderr,
            "SESSION: child exited code=%d action=%d\n",
            exit_code,
            result->action);

    } else if (
        WIFSIGNALED(status)) {

        signal_number =
            WTERMSIG(status);

        result->signaled = 1;

        result->signal_number =
            signal_number;

        result->action =
            FLIP_SESSION_ACTION_ERROR;

        fprintf(
            stderr,
            "SESSION: child signaled signal=%d\n",
            signal_number);
    }

    const char *session_state;

    if (result->signaled) {
        session_state =
            "signaled";

    } else if (
        result->action ==
            FLIP_SESSION_ACTION_SUSPEND) {

        session_state =
            "suspended";

    } else {
        session_state =
            "exited";
    }

    write_session_state(
        session_state,
        program,
        metadata,
        pid,
        exit_code,
        signal_number,
        started,
        now_epoch());

    if (restore_hardware(drm) < 0)
        return -1;

    fprintf(
        stderr,
        "SESSION: return_to_frontend\n");

    return 0;
}

static int session_process_matches(
    int pid,
    const char *program)
{
    if (pid <= 0 ||
        !program ||
        !program[0])
        return 0;

    char proc_exe_path[64];

    snprintf(
        proc_exe_path,
        sizeof(proc_exe_path),
        "/proc/%d/exe",
        pid);

    char proc_target[PATH_MAX];

    ssize_t length =
        readlink(
            proc_exe_path,
            proc_target,
            sizeof(proc_target) - 1);

    if (length < 0)
        return 0;

    proc_target[length] = '\0';

    char expected_real[PATH_MAX];
    char actual_real[PATH_MAX];

    if (!realpath(
            program,
            expected_real))
        return 0;

    if (!realpath(
            proc_target,
            actual_real))
        return 0;

    return strcmp(
        expected_real,
        actual_real) == 0;
}

int flip_session_read_current(
    flip_session_state_t *session)
{
    if (!session)
        return -1;

    memset(
        session,
        0,
        sizeof(*session));

    FILE *f =
        fopen(
            SESSION_FILE,
            "r");

    if (!f) {

        if (errno == ENOENT)
            return 0;

        fprintf(
            stderr,
            "SESSION_READ: fopen failed: %s\n",
            strerror(errno));

        return -1;
    }

    int version = 0;

    int have_version = 0;
    int have_state = 0;
    int have_system = 0;
    int have_rom = 0;
    int have_program = 0;
    int have_pid = 0;
    int have_started = 0;
    int have_ended = 0;
    int have_exit_code = 0;
    int have_signal = 0;

    int malformed = 0;

    char line[1024];

    while (fgets(
               line,
               sizeof(line),
               f)) {

        char *newline =
            strchr(
                line,
                '\n');

        if (newline) {

            *newline = '\0';

        } else if (!feof(f)) {

            /*
             * The physical line exceeded our parser buffer.
             */
            malformed = 1;

            int c;

            while ((c = fgetc(f)) != '\n' &&
                   c != EOF) {
            }

            continue;
        }

        if (strncmp(
                line,
                "version=",
                8) == 0) {

            if (have_version ||
                parse_int_value(
                    line + 8,
                    &version) < 0) {

                malformed = 1;
            }

            have_version = 1;

        } else if (
            strncmp(
                line,
                "state=",
                6) == 0) {

            if (have_state)
                malformed = 1;

            copy_string(
                session->state,
                sizeof(session->state),
                line + 6);

            have_state = 1;

        } else if (
            strncmp(
                line,
                "system=",
                7) == 0) {

            if (have_system)
                malformed = 1;

            copy_string(
                session->system,
                sizeof(session->system),
                line + 7);

            have_system = 1;

        } else if (
            strncmp(
                line,
                "rom=",
                4) == 0) {

            if (have_rom)
                malformed = 1;

            copy_string(
                session->rom,
                sizeof(session->rom),
                line + 4);

            have_rom = 1;

        } else if (
            strncmp(
                line,
                "program=",
                8) == 0) {

            if (have_program)
                malformed = 1;

            copy_string(
                session->program,
                sizeof(session->program),
                line + 8);

            have_program = 1;

        } else if (
            strncmp(
                line,
                "pid=",
                4) == 0) {

            if (have_pid ||
                parse_int_value(
                    line + 4,
                    &session->pid) < 0) {

                malformed = 1;
            }

            have_pid = 1;

        } else if (
            strncmp(
                line,
                "started=",
                8) == 0) {

            if (have_started ||
                parse_ll_value(
                    line + 8,
                    &session->started) < 0) {

                malformed = 1;
            }

            have_started = 1;

        } else if (
            strncmp(
                line,
                "ended=",
                6) == 0) {

            if (have_ended ||
                parse_ll_value(
                    line + 6,
                    &session->ended) < 0) {

                malformed = 1;
            }

            have_ended = 1;

        } else if (
            strncmp(
                line,
                "exit_code=",
                10) == 0) {

            if (have_exit_code ||
                parse_int_value(
                    line + 10,
                    &session->exit_code) < 0) {

                malformed = 1;
            }

            have_exit_code = 1;

        } else if (
            strncmp(
                line,
                "signal=",
                7) == 0) {

            if (have_signal ||
                parse_int_value(
                    line + 7,
                    &session->signal_number) < 0) {

                malformed = 1;
            }

            have_signal = 1;
        }
    }

    fclose(f);

    if (!have_version ||
        version != 1 ||
        !have_state ||
        !session->state[0] ||
        !have_system ||
        !session->system[0] ||
        !have_rom ||
        !session->rom[0] ||
        !have_program ||
        !session->program[0] ||
        !have_pid ||
        session->pid < 0 ||
        !have_started ||
        session->started < 0 ||
        !have_ended ||
        session->ended < 0 ||
        !have_exit_code ||
        !have_signal) {

        malformed = 1;
    }

    if (malformed) {

        fprintf(
            stderr,
            "SESSION_READ: malformed current session\n");

        memset(
            session,
            0,
            sizeof(*session));

        return -1;
    }

    session->valid = 1;

    fprintf(
        stderr,
        "SESSION_READ: state=%s system=%s rom=%s pid=%d\n",
        session->state,
        session->system,
        session->rom,
        session->pid);

    return 1;
}

static int quarantine_invalid_session(void)
{
    char invalid_path[512];

    int written =
        snprintf(
            invalid_path,
            sizeof(invalid_path),
            "%s.invalid.%lld",
            SESSION_FILE,
            now_epoch());

    if (written < 0 ||
        (size_t)written >= sizeof(invalid_path)) {

        fprintf(
            stderr,
            "SESSION_RECOVER: invalid quarantine path\n");

        return -1;
    }

    if (rename(
            SESSION_FILE,
            invalid_path) < 0) {

        fprintf(
            stderr,
            "SESSION_RECOVER: quarantine failed: %s\n",
            strerror(errno));

        return -1;
    }

    fprintf(
        stderr,
        "SESSION_RECOVER: invalid session quarantined=%s savestate=preserved\n",
        invalid_path);

    return 1;
}

int flip_session_recover_current(void)
{
    flip_session_state_t session;

    int read_result =
        flip_session_read_current(
            &session);

    if (read_result == 0)
        return 0;

    if (read_result < 0) {

        /*
         * Structurally invalid metadata cannot be repaired
         * safely because we cannot trust its fields.
         *
         * Preserve the exact record for diagnostics and
         * continue startup with no active session.
         */
        return quarantine_invalid_session();
    }

    /*
     * Only "running" records can represent an abandoned
     * child process. Suspended/closed/exited/etc. are
     * persistent lifecycle metadata and are left alone.
     */
    if (strcmp(
            session.state,
            "running") != 0)
        return 0;

    if (session_process_matches(
            session.pid,
            session.program)) {

        fprintf(
            stderr,
            "SESSION_RECOVER: running process valid pid=%d\n",
            session.pid);

        return 0;
    }

    const flip_system_t *system =
        flip_system_find(
            session.system);

    if (!system) {

        fprintf(
            stderr,
            "SESSION_RECOVER: invalid system=%s quarantining\n",
            session.system);

        return quarantine_invalid_session();
    }

    /*
     * A structurally valid session is still unusable if its
     * ROM has disappeared or is no longer valid content for
     * the recorded system.
     *
     * Never manufacture an interrupted session for content
     * that cannot actually be launched.
     */
    if (!session.rom[0] ||
        access(
            session.rom,
            R_OK) != 0 ||
        !flip_system_supports_path(
            system,
            session.rom)) {

        fprintf(
            stderr,
            "SESSION_RECOVER: invalid rom=%s quarantining\n",
            session.rom);

        return quarantine_invalid_session();
    }

    flip_session_metadata_t metadata = {
        .system = system,
        .rom_path = session.rom
    };

    if (write_session_state(
            "interrupted",
            session.program,
            &metadata,
            (pid_t)session.pid,
            session.exit_code,
            session.signal_number,
            session.started,
            now_epoch()) < 0) {

        return -1;
    }

    fprintf(
        stderr,
        "SESSION_RECOVER: interrupted rom=%s pid=%d savestate=preserved\n",
        session.rom,
        session.pid);

    return 1;
}

int flip_session_close_suspended_game(
    const char *system_id,
    const char *rom_path)
{
    if (!system_id ||
        !system_id[0] ||
        !rom_path ||
        !rom_path[0]) {

        fprintf(
            stderr,
            "SESSION_CLOSE_TARGET: invalid target\n");

        return -1;
    }

    flip_session_state_t session;

    int read_result =
        flip_session_read_current(
            &session);

    /*
     * No current lifecycle record is not an error for a
     * target-specific close. The selected retained session may
     * simply be an older GameSwitcher entry.
     */
    if (read_result == 0)
        return 0;

    if (read_result < 0) {

        fprintf(
            stderr,
            "SESSION_CLOSE_TARGET: invalid current session\n");

        return -1;
    }

    if (strcmp(
            session.system,
            system_id) != 0 ||
        strcmp(
            session.rom,
            rom_path) != 0) {

        fprintf(
            stderr,
            "SESSION_CLOSE_TARGET: current session does not match target "
            "current_system=%s target_system=%s current_rom=%s target_rom=%s\n",
            session.system,
            system_id,
            session.rom,
            rom_path);

        return 0;
    }

    if (strcmp(
            session.state,
            "suspended") != 0) {

        fprintf(
            stderr,
            "SESSION_CLOSE_TARGET: matching session not suspended state=%s\n",
            session.state);

        return 0;
    }

    const flip_system_t *session_system =
        flip_system_find(
            session.system);

    if (!session_system) {

        fprintf(
            stderr,
            "SESSION_CLOSE_TARGET: unknown system=%s\n",
            session.system);

        return -1;
    }

    flip_session_metadata_t metadata = {
        .system = session_system,
        .rom_path = session.rom
    };

    if (write_session_state(
            "closed",
            session.program,
            &metadata,
            (pid_t)session.pid,
            0,
            0,
            session.started,
            now_epoch()) < 0) {

        return -1;
    }

    fprintf(
        stderr,
        "SESSION_CLOSE_TARGET: closed system=%s rom=%s savestate=preserved\n",
        session.system,
        session.rom);

    return 1;
}


int flip_session_close_suspended(void)
{
    flip_session_state_t session;

    int read_result =
        flip_session_read_current(
            &session);

    if (read_result == 0) {

        fprintf(
            stderr,
            "SESSION_CLOSE: no current session\n");

        return -1;
    }

    if (read_result < 0) {

        fprintf(
            stderr,
            "SESSION_CLOSE: invalid current session\n");

        return -1;
    }

    if (strcmp(
            session.state,
            "suspended") != 0) {

        fprintf(
            stderr,
            "SESSION_CLOSE: current session not suspended state=%s\n",
            session.state);

        return 0;
    }

    const flip_system_t *session_system =
        flip_system_find(
            session.system);

    if (!session_system) {

        fprintf(
            stderr,
            "SESSION_CLOSE: unknown system=%s\n",
            session.system);

        return -1;
    }

    flip_session_metadata_t metadata = {
        .system = session_system,
        .rom_path = session.rom
    };

    if (write_session_state(
            "closed",
            session.program,
            &metadata,
            (pid_t)session.pid,
            0,
            0,
            session.started,
            now_epoch()) < 0) {

        return -1;
    }

    fprintf(
        stderr,
        "SESSION_CLOSE: closed rom=%s savestate=preserved\n",
        session.rom);

    return 1;
}
