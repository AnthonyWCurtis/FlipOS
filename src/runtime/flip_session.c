#define _GNU_SOURCE

#include "flip_session.h"
#include "flip_cpu_profile.h"
#include "flip_paths.h"
#include "flip_exit_protocol.h"
#include "flip_session_registry.h"
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

#define RECENT_MAX 10

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
    flip_session_state_t persisted;
    memset(
        &persisted,
        0,
        sizeof(persisted));

    persisted.valid = 1;

    if (copy_string(
            persisted.state,
            sizeof(persisted.state),
            state ? state : "unknown") != 0 ||
        !metadata ||
        !metadata->system ||
        !metadata->system->id ||
        copy_string(
            persisted.system,
            sizeof(persisted.system),
            metadata->system->id) != 0 ||
        !metadata->rom_path ||
        copy_string(
            persisted.rom,
            sizeof(persisted.rom),
            metadata->rom_path) != 0 ||
        !program ||
        copy_string(
            persisted.program,
            sizeof(persisted.program),
            program) != 0) {

        fprintf(
            stderr,
            "SESSION_STATE: invalid metadata\n");

        return -1;
    }

    persisted.pid = (int)pid;
    persisted.started = started;
    persisted.ended = ended;
    persisted.exit_code = exit_code;
    persisted.signal_number = signal_number;

    if (flip_session_persistence_write_current(
            &persisted) < 0) {

        fprintf(
            stderr,
            "SESSION_STATE: persist failed: %s\n",
            strerror(errno));

        return -1;
    }

    fprintf(
        stderr,
        "SESSION_STATE: state=%s rom=%s\n",
        persisted.state,
        persisted.rom);

    /*
     * Phase 6A mirrors lifecycle state into the multi-session
     * registry without making it authoritative yet.
     *
     * The hardware-tested Phase 5C current-session record above
     * remains the source of truth. Registry failures are logged
     * but must not break launch/save/suspend/resume behaviour.
     */
    if (metadata &&
        metadata->system &&
        metadata->system->id &&
        metadata->rom_path &&
        program &&
        state) {

        int registry_result = 0;

        if (strcmp(state, "exited") == 0 ||
            strcmp(state, "closed") == 0) {

            registry_result =
                flip_session_registry_remove(
                    metadata->system->id,
                    metadata->rom_path);

        } else {

            flip_session_registry_entry_t entry;

            memset(
                &entry,
                0,
                sizeof(entry));

            entry.valid = 1;

            copy_string(
                entry.system,
                sizeof(entry.system),
                metadata->system->id);

            copy_string(
                entry.rom,
                sizeof(entry.rom),
                metadata->rom_path);

            copy_string(
                entry.program,
                sizeof(entry.program),
                program);

            copy_string(
                entry.state,
                sizeof(entry.state),
                state);

            entry.started = started;
            entry.updated =
                ended > 0
                    ? ended
                    : now_epoch();

            registry_result =
                flip_session_registry_upsert(
                    &entry);
        }

        if (registry_result < 0) {
            fprintf(
                stderr,
                "SESSION_REGISTRY: mirror failed state=%s rom=%s\n",
                state,
                metadata->rom_path);
        }
    }

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
            FLIP_RECENT_ROOT,
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
            FLIP_RECENT_FILE,
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
        FLIP_RECENT_FILE);

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
            FLIP_RECENT_FILE) < 0) {

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
            FLIP_RECENT_FILE,
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
        FLIP_RECENT_FILE);

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
            FLIP_RECENT_FILE) < 0) {

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
            FLIP_RECENT_FILE,
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
        FLIP_RECENT_FILE);

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
            FLIP_RECENT_FILE) < 0) {

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

static int session_cpu_profile_apply(
    const flip_session_metadata_t *metadata,
    flip_cpu_profile_t *profile)
{
    if (!profile)
        return -1;

    memset(profile, 0, sizeof(*profile));

    if (!metadata ||
        !metadata->system ||
        !metadata->system->id)
        return 0;

    int result =
        flip_cpu_profile_apply(
            metadata->system->id,
            profile);

    if (result > 0) {
        fprintf(
            stderr,
            "SESSION_CPU_PROFILE: applied system=%s max_khz=%u original_max_khz=%u\n",
            metadata->system->id,
            profile->applied_max_khz,
            profile->original.max_khz);
    } else if (result < 0) {
        fprintf(
            stderr,
            "SESSION_CPU_PROFILE: apply failed system=%s; continuing existing policy\n",
            metadata->system->id);
    }

    return result;
}

static void session_cpu_profile_restore(
    flip_cpu_profile_t *profile)
{
    if (!profile || !profile->active)
        return;

    unsigned original_max_khz =
        profile->original.max_khz;

    if (flip_cpu_profile_restore(profile) < 0) {
        fprintf(
            stderr,
            "SESSION_CPU_PROFILE: restore failed target_max_khz=%u\n",
            original_max_khz);
        return;
    }

    fprintf(
        stderr,
        "SESSION_CPU_PROFILE: restored max_khz=%u\n",
        original_max_khz);
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

    flip_cpu_profile_t cpu_profile;
    session_cpu_profile_apply(
        metadata,
        &cpu_profile);

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

        session_cpu_profile_restore(
            &cpu_profile);

        restore_hardware(drm);

        return -1;
    }

    if (pid == 0) {

        if (cpu_profile.active) {
            char dynamic_max_khz[32];
            snprintf(
                dynamic_max_khz,
                sizeof(dynamic_max_khz),
                "%u",
                cpu_profile.original.max_khz);
            setenv(
                "FLIP_CPU_DYNAMIC_MAX_KHZ",
                dynamic_max_khz,
                1);
        } else {
            unsetenv(
                "FLIP_CPU_DYNAMIC_MAX_KHZ");
        }

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

    struct timespec active_started;
    int active_clock_valid =
        clock_gettime(
            CLOCK_MONOTONIC,
            &active_started) == 0;

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

        session_cpu_profile_restore(
            &cpu_profile);

        if (restore_hardware(drm) < 0)
            return -1;

        return -1;
    }

    if (active_clock_valid) {
        struct timespec active_ended;

        if (clock_gettime(
                CLOCK_MONOTONIC,
                &active_ended) == 0) {

            time_t active_sec =
                active_ended.tv_sec -
                active_started.tv_sec;

            long active_nsec =
                active_ended.tv_nsec -
                active_started.tv_nsec;

            if (active_nsec < 0) {
                active_sec--;
                active_nsec += 1000000000L;
            }

            if (active_sec >= 0) {
                result->active_seconds =
                    (unsigned long long)active_sec;

                if (active_nsec >= 500000000L)
                    result->active_seconds++;
            }
        }
    }

    result->exited = 1;

    int exit_code = 0;
    int signal_number = 0;

    if (WIFEXITED(status)) {

        exit_code =
            WEXITSTATUS(status);

        result->exit_code =
            exit_code;

        switch (flip_exit_protocol_classify(
                    exit_code)) {
        case FLIP_EXIT_PROTOCOL_SUSPEND:
            result->action =
                FLIP_SESSION_ACTION_SUSPEND;
            break;

        case FLIP_EXIT_PROTOCOL_LOW_BATTERY:
            result->action =
                FLIP_SESSION_ACTION_LOW_BATTERY;
            break;

        case FLIP_EXIT_PROTOCOL_NORMAL:
            result->action =
                FLIP_SESSION_ACTION_EXIT;
            break;

        case FLIP_EXIT_PROTOCOL_ERROR:
        default:
            result->action =
                FLIP_SESSION_ACTION_ERROR;
            break;
        }

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
        result->action == FLIP_SESSION_ACTION_SUSPEND ||
        result->action == FLIP_SESSION_ACTION_LOW_BATTERY) {

        /*
         * Low-battery exits are deliberately recoverable: the emulator has
         * already committed SRAM + resume state before requesting poweroff.
         */
        session_state =
            "suspended";

    } else if (
        result->action ==
            FLIP_SESSION_ACTION_ERROR) {

        session_state =
            "error";

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

    session_cpu_profile_restore(
        &cpu_profile);

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
    int result =
        flip_session_persistence_read_current(
            session);

    if (result < 0) {
        fprintf(
            stderr,
            "SESSION_READ: malformed current session\n");

        return -1;
    }

    if (result == 0)
        return 0;

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
    char invalid_path[1024];

    int result =
        flip_session_persistence_quarantine_current(
            invalid_path,
            sizeof(invalid_path));

    if (result < 0) {
        fprintf(
            stderr,
            "SESSION_RECOVER: quarantine failed: %s\n",
            strerror(errno));

        return -1;
    }

    if (result == 0)
        return 0;

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


int flip_session_close_registered(
    const flip_system_t *system,
    const char *rom_path)
{
    if (!system ||
        !system->id ||
        !rom_path ||
        !rom_path[0])
        return -1;

    flip_session_registry_entry_t entry;

    int find_result =
        flip_session_registry_find(
            system->id,
            rom_path,
            &entry);

    if (find_result <= 0)
        return find_result;

    int closable =
        strcmp(entry.state, "suspended") == 0 ||
        strcmp(entry.state, "interrupted") == 0 ||
        strcmp(entry.state, "signaled") == 0 ||
        strcmp(entry.state, "error") == 0;

    if (!closable) {

        fprintf(
            stderr,
            "SESSION_CLOSE: registry target not closable state=%s rom=%s\n",
            entry.state,
            rom_path);

        return 0;
    }

    flip_session_state_t current;

    int current_result =
        flip_session_read_current(
            &current);

    if (current_result > 0 &&
        strcmp(current.state, "suspended") == 0 &&
        strcmp(current.system, system->id) == 0 &&
        strcmp(current.rom, rom_path) == 0) {

        return flip_session_close_suspended();
    }

    if (flip_session_registry_remove(
            system->id,
            rom_path) < 0) {

        fprintf(
            stderr,
            "SESSION_CLOSE: registry remove failed rom=%s\n",
            rom_path);

        return -1;
    }

    fprintf(
        stderr,
        "SESSION_CLOSE: closed registered rom=%s savestate=preserved\n",
        rom_path);

    return 1;
}


static int registry_state_known(
    const char *state)
{
    return state &&
           (strcmp(state, "running") == 0 ||
            strcmp(state, "suspended") == 0 ||
            strcmp(state, "interrupted") == 0 ||
            strcmp(state, "signaled") == 0 ||
            strcmp(state, "error") == 0 ||
            strcmp(state, "exited") == 0 ||
            strcmp(state, "closed") == 0);
}

int flip_session_reconcile_registry(void)
{
    flip_session_registry_entry_t entries[
        FLIP_SESSION_REGISTRY_MAX];

    size_t count = 0;

    if (flip_session_registry_load(
            entries,
            FLIP_SESSION_REGISTRY_MAX,
            &count) < 0) {

        fprintf(
            stderr,
            "SESSION_REGISTRY: reconcile load failed\n");

        return -1;
    }

    flip_session_state_t current;
    int current_result =
        flip_session_read_current(
            &current);

    int changed = 0;

    for (size_t i = 0; i < count; ++i) {

        flip_session_registry_entry_t *entry =
            &entries[i];

        const flip_system_t *system =
            flip_system_find(
                entry->system);

        int launchable =
            system &&
            access(
                entry->rom,
                R_OK) == 0 &&
            flip_system_supports_path(
                system,
                entry->rom);

        if (!launchable ||
            !registry_state_known(
                entry->state) ||
            strcmp(
                entry->state,
                "exited") == 0 ||
            strcmp(
                entry->state,
                "closed") == 0) {

            int remove_result =
                flip_session_registry_remove(
                    entry->system,
                    entry->rom);

            if (remove_result < 0)
                return -1;

            if (remove_result > 0) {
                changed++;

                fprintf(
                    stderr,
                    "SESSION_REGISTRY: pruned state=%s system=%s rom=%s\n",
                    entry->state,
                    entry->system,
                    entry->rom);
            }

            continue;
        }

        if (strcmp(
                entry->state,
                "running") != 0)
            continue;

        int current_matches =
            current_result > 0 &&
            strcmp(
                current.state,
                "running") == 0 &&
            strcmp(
                current.system,
                entry->system) == 0 &&
            strcmp(
                current.rom,
                entry->rom) == 0;

        if (current_matches)
            continue;

        snprintf(
            entry->state,
            sizeof(entry->state),
            "%s",
            "interrupted");

        entry->updated =
            now_epoch();

        if (flip_session_registry_upsert(
                entry) < 0)
            return -1;

        changed++;

        fprintf(
            stderr,
            "SESSION_REGISTRY: recovered interrupted system=%s rom=%s\n",
            entry->system,
            entry->rom);
    }

    return changed;
}
