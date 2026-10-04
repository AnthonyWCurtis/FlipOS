#define _GNU_SOURCE

#include "flip_session_persistence.h"
#include "flip_paths.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define SESSION_VERSION 1

static int ensure_root(void)
{
    if (mkdir(
            FLIP_STATE_DATA_ROOT,
            0777) < 0 &&
        errno != EEXIST)
        return -1;

    if (mkdir(
            FLIP_SESSION_ROOT,
            0777) < 0 &&
        errno != EEXIST)
        return -1;

    return 0;
}

static int text_is_safe(
    const char *text)
{
    return text &&
           text[0] &&
           !strchr(text, '\n') &&
           !strchr(text, '\r');
}

static int copy_field(
    char *dest,
    size_t dest_size,
    const char *source)
{
    if (!dest ||
        dest_size == 0 ||
        !source)
        return -1;

    size_t length = strlen(source);

    if (length >= dest_size)
        return -1;

    memcpy(
        dest,
        source,
        length + 1);

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

static int validate_state(
    const flip_session_state_t *state)
{
    if (!state ||
        !text_is_safe(state->state) ||
        !text_is_safe(state->system) ||
        !text_is_safe(state->rom) ||
        !text_is_safe(state->program) ||
        state->pid < 0 ||
        state->started < 0 ||
        state->ended < 0)
        return -1;

    return 0;
}

int flip_session_persistence_write_current(
    const flip_session_state_t *state)
{
    if (validate_state(state) < 0)
        return -1;

    if (ensure_root() < 0)
        return -1;

    char tmp_path[1024];

    int written = snprintf(
        tmp_path,
        sizeof(tmp_path),
        "%s.tmp",
        FLIP_SESSION_FILE);

    if (written < 0 ||
        (size_t)written >= sizeof(tmp_path))
        return -1;

    FILE *f =
        fopen(
            tmp_path,
            "w");

    if (!f)
        return -1;

    int failed = 0;
    int saved_errno = 0;

    if (fprintf(
            f,
            "version=%d\n"
            "state=%s\n"
            "system=%s\n"
            "rom=%s\n"
            "program=%s\n"
            "pid=%d\n"
            "started=%lld\n"
            "ended=%lld\n"
            "exit_code=%d\n"
            "signal=%d\n",
            SESSION_VERSION,
            state->state,
            state->system,
            state->rom,
            state->program,
            state->pid,
            state->started,
            state->ended,
            state->exit_code,
            state->signal_number) < 0) {

        failed = 1;
        saved_errno = errno;
    }

    if (!failed &&
        fflush(f) != 0) {
        failed = 1;
        saved_errno = errno;
    }

    if (!failed &&
        fsync(fileno(f)) < 0) {
        failed = 1;
        saved_errno = errno;
    }

    if (fclose(f) != 0 &&
        !failed) {
        failed = 1;
        saved_errno = errno;
    }

    if (failed) {
        unlink(tmp_path);
        errno = saved_errno;
        return -1;
    }

    if (rename(
            tmp_path,
            FLIP_SESSION_FILE) < 0) {
        int rename_errno = errno;
        unlink(tmp_path);
        errno = rename_errno;
        return -1;
    }

    return 0;
}

int flip_session_persistence_read_current(
    flip_session_state_t *state)
{
    if (!state)
        return -1;

    memset(
        state,
        0,
        sizeof(*state));

    FILE *f =
        fopen(
            FLIP_SESSION_FILE,
            "r");

    if (!f) {
        if (errno == ENOENT)
            return 0;

        return -1;
    }

    int version = 0;

    unsigned fields = 0;
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

            if ((fields & 0x001) ||
                parse_int_value(
                    line + 8,
                    &version) < 0)
                malformed = 1;

            fields |= 0x001;

        } else if (strncmp(
                       line,
                       "state=",
                       6) == 0) {

            if ((fields & 0x002) ||
                copy_field(
                    state->state,
                    sizeof(state->state),
                    line + 6) < 0)
                malformed = 1;

            fields |= 0x002;

        } else if (strncmp(
                       line,
                       "system=",
                       7) == 0) {

            if ((fields & 0x004) ||
                copy_field(
                    state->system,
                    sizeof(state->system),
                    line + 7) < 0)
                malformed = 1;

            fields |= 0x004;

        } else if (strncmp(
                       line,
                       "rom=",
                       4) == 0) {

            if ((fields & 0x008) ||
                copy_field(
                    state->rom,
                    sizeof(state->rom),
                    line + 4) < 0)
                malformed = 1;

            fields |= 0x008;

        } else if (strncmp(
                       line,
                       "program=",
                       8) == 0) {

            if ((fields & 0x010) ||
                copy_field(
                    state->program,
                    sizeof(state->program),
                    line + 8) < 0)
                malformed = 1;

            fields |= 0x010;

        } else if (strncmp(
                       line,
                       "pid=",
                       4) == 0) {

            if ((fields & 0x020) ||
                parse_int_value(
                    line + 4,
                    &state->pid) < 0)
                malformed = 1;

            fields |= 0x020;

        } else if (strncmp(
                       line,
                       "started=",
                       8) == 0) {

            if ((fields & 0x040) ||
                parse_ll_value(
                    line + 8,
                    &state->started) < 0)
                malformed = 1;

            fields |= 0x040;

        } else if (strncmp(
                       line,
                       "ended=",
                       6) == 0) {

            if ((fields & 0x080) ||
                parse_ll_value(
                    line + 6,
                    &state->ended) < 0)
                malformed = 1;

            fields |= 0x080;

        } else if (strncmp(
                       line,
                       "exit_code=",
                       10) == 0) {

            if ((fields & 0x100) ||
                parse_int_value(
                    line + 10,
                    &state->exit_code) < 0)
                malformed = 1;

            fields |= 0x100;

        } else if (strncmp(
                       line,
                       "signal=",
                       7) == 0) {

            if ((fields & 0x200) ||
                parse_int_value(
                    line + 7,
                    &state->signal_number) < 0)
                malformed = 1;

            fields |= 0x200;

        } else if (line[0]) {
            malformed = 1;
        }
    }

    if (ferror(f))
        malformed = 1;

    fclose(f);

    if (fields != 0x3ff ||
        version != SESSION_VERSION ||
        validate_state(state) < 0)
        malformed = 1;

    if (malformed) {
        memset(
            state,
            0,
            sizeof(*state));
        return -1;
    }

    state->valid = 1;
    return 1;
}

int flip_session_persistence_quarantine_current(
    char *quarantined_path,
    size_t path_size)
{
    if (!quarantined_path ||
        path_size == 0)
        return -1;

    quarantined_path[0] = '\0';

    if (access(
            FLIP_SESSION_FILE,
            F_OK) != 0) {
        return errno == ENOENT
            ? 0
            : -1;
    }

    long long epoch =
        (long long)time(NULL);

    int written = snprintf(
        quarantined_path,
        path_size,
        "%s.invalid.%lld",
        FLIP_SESSION_FILE,
        epoch);

    if (written < 0 ||
        (size_t)written >= path_size) {
        quarantined_path[0] = '\0';
        return -1;
    }

    if (rename(
            FLIP_SESSION_FILE,
            quarantined_path) < 0) {
        quarantined_path[0] = '\0';
        return -1;
    }

    return 1;
}
