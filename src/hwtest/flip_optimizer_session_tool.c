#include "flip_optimizer_calibration.h"
#include "flip_optimizer_supervisor.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define SESSION_FILE \
    FLIP_OPTIMIZER_CALIBRATION_ROOT "/auto-session"
#define SESSION_TMP \
    FLIP_OPTIMIZER_CALIBRATION_ROOT "/auto-session.tmp"
#define CANDIDATE_PATH \
    "/mnt/SDCARD/flipos-dev/bin/flip-optimizer-candidate"
#define DEFAULT_DURATION_SECONDS 20U

typedef struct {
    int active;
    int paused;
    unsigned duration_seconds;
    unsigned target_complete_count;
} session_state_t;

static int fsync_parent(void)
{
    int fd = open(
        FLIP_OPTIMIZER_CALIBRATION_ROOT,
        O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd < 0)
        return -1;
    int rc = fsync(fd);
    close(fd);
    return rc;
}

static int save_session(const session_state_t *state)
{
    int fd = open(
        SESSION_TMP,
        O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,
        0644);
    if (fd < 0)
        return -1;

    char buf[128];
    int n = snprintf(
        buf,
        sizeof(buf),
        "version=1\nactive=%d\npaused=%d\nduration_seconds=%u\ntarget_complete_count=%u\n",
        state->active ? 1 : 0,
        state->paused ? 1 : 0,
        state->duration_seconds,
        state->target_complete_count);

    if (n <= 0 || (size_t)n >= sizeof(buf)) {
        close(fd);
        return -1;
    }

    ssize_t written = write(fd, buf, (size_t)n);
    if (written != n || fsync(fd) < 0) {
        close(fd);
        return -1;
    }

    if (close(fd) < 0)
        return -1;

    if (rename(SESSION_TMP, SESSION_FILE) < 0)
        return -1;

    return fsync_parent();
}

static int load_session(session_state_t *state)
{
    memset(state, 0, sizeof(*state));

    FILE *f = fopen(SESSION_FILE, "r");
    if (!f) {
        if (errno == ENOENT)
            return 0;
        return -1;
    }

    int version = 0;
    char line[128];

    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "version=%d", &version) == 1)
            continue;
        if (sscanf(line, "active=%d", &state->active) == 1)
            continue;
        if (sscanf(line, "paused=%d", &state->paused) == 1)
            continue;
        if (sscanf(
                line,
                "duration_seconds=%u",
                &state->duration_seconds) == 1)
            continue;
        if (sscanf(
                line,
                "target_complete_count=%u",
                &state->target_complete_count) == 1)
            continue;
    }

    int failed = ferror(f);
    fclose(f);

    if (failed ||
        version != 1 ||
        (state->active != 0 && state->active != 1) ||
        (state->paused != 0 && state->paused != 1) ||
        state->duration_seconds == 0 ||
        state->duration_seconds > 120 ||
        state->target_complete_count == 0 ||
        state->target_complete_count >
            FLIP_OPTIMIZER_CALIBRATION_MAX_OPPS)
        return -1;

    return 1;
}

static int clear_session(void)
{
    if (unlink(SESSION_FILE) < 0 && errno != ENOENT)
        return -1;
    (void)unlink(SESSION_TMP);
    return fsync_parent();
}

static unsigned complete_count(
    const flip_optimizer_calibration_t *calibration)
{
    unsigned count = 0;
    for (size_t i = 0; i < calibration->opp_count; ++i) {
        if (calibration->opps[i].complete)
            ++count;
    }
    return count;
}

static int print_status(void)
{
    session_state_t state;
    int session_rc = load_session(&state);

    if (session_rc < 0)
        return -1;

    printf("session_present=%d\n", session_rc == 1 ? 1 : 0);

    if (session_rc == 1) {
        printf("session_active=%d\n", state.active);
        printf("session_paused=%d\n", state.paused);
        printf(
            "duration_seconds=%u\n",
            state.duration_seconds);
        printf(
            "target_complete_count=%u\n",
            state.target_complete_count);
    }

    flip_optimizer_calibration_t calibration;
    if (flip_optimizer_calibration_load(
            &calibration) == 0) {
        printf(
            "complete_opps=%u\n",
            complete_count(&calibration));
        printf("total_opps=%zu\n", calibration.opp_count);
        printf(
            "calibration_complete=%d\n",
            flip_optimizer_calibration_is_complete(
                &calibration));
        printf("pending=%d\n", calibration.pending);
    }

    return 0;
}

static int start_session(
    unsigned duration_seconds,
    int one_opp_only)
{
    flip_optimizer_calibration_t calibration;
    if (flip_optimizer_calibration_load(
            &calibration) < 0)
        return -1;

    if (flip_optimizer_calibration_is_complete(
            &calibration))
        return -1;

    if (calibration.pending)
        return -1;

    unsigned completed =
        complete_count(&calibration);
    unsigned target =
        one_opp_only
            ? completed + 1U
            : (unsigned)calibration.opp_count;

    if (target > calibration.opp_count)
        target = (unsigned)calibration.opp_count;

    session_state_t state = {
        .active = 1,
        .paused = 0,
        .duration_seconds = duration_seconds,
        .target_complete_count = target,
    };

    if (save_session(&state) < 0)
        return -1;

    printf(
        "OPTIMIZER_SESSION: STARTED duration_s=%u complete=%u/%zu target=%u\n",
        duration_seconds,
        completed,
        calibration.opp_count,
        target);
    return 0;
}

static int pause_session(
    session_state_t *state,
    const char *reason)
{
    state->active = 1;
    state->paused = 1;

    if (save_session(state) < 0)
        return -1;

    printf(
        "OPTIMIZER_SESSION: PAUSED reason=%s\n",
        reason);
    return 0;
}

static int boot_resume(void)
{
    session_state_t state;
    int session_rc = load_session(&state);

    if (session_rc <= 0)
        return session_rc < 0 ? -1 : 0;

    if (!state.active || state.paused) {
        printf(
            "OPTIMIZER_SESSION: NOT_RESUMED active=%d paused=%d\n",
            state.active,
            state.paused);
        return 0;
    }

    if (flip_optimizer_supervisor_runtime_stock_safe()) {
        return pause_session(
            &state,
            "runtime-stock-safe");
    }

    flip_optimizer_calibration_t calibration;
    if (flip_optimizer_calibration_load(
            &calibration) < 0) {
        return pause_session(
            &state,
            "calibration-load");
    }

    if (calibration.pending) {
        return pause_session(
            &state,
            "pending-after-recovery");
    }

    unsigned completed =
        complete_count(&calibration);

    if (completed >= state.target_complete_count ||
        flip_optimizer_calibration_is_complete(
            &calibration)) {
        printf(
            "OPTIMIZER_SESSION: COMPLETE complete=%u/%zu target=%u\n",
            completed,
            calibration.opp_count,
            state.target_complete_count);
        return clear_session();
    }

    printf(
        "OPTIMIZER_SESSION: RESUME complete=%u/%zu target=%u duration_s=%u\n",
        completed,
        calibration.opp_count,
        state.target_complete_count,
        state.duration_seconds);
    fflush(stdout);

    char duration[16];
    snprintf(
        duration,
        sizeof(duration),
        "%u",
        state.duration_seconds);

    execl(
        CANDIDATE_PATH,
        CANDIDATE_PATH,
        "live-once",
        duration,
        (char *)NULL);

    return pause_session(
        &state,
        "candidate-exec");
}

static void usage(const char *program)
{
    fprintf(
        stderr,
        "usage: %s start [SECONDS] | start-one [SECONDS] | stop | status | boot-resume\n",
        program);
}

int main(int argc, char **argv)
{
    if (argc >= 2 &&
        (strcmp(argv[1], "start") == 0 ||
         strcmp(argv[1], "start-one") == 0)) {
        unsigned duration = DEFAULT_DURATION_SECONDS;
        int one_opp_only =
            strcmp(argv[1], "start-one") == 0;

        if (argc == 3) {
            char *end = NULL;
            unsigned long parsed =
                strtoul(argv[2], &end, 10);

            if (!end ||
                *end != '\0' ||
                parsed == 0 ||
                parsed > 120) {
                usage(argv[0]);
                return EXIT_FAILURE;
            }

            duration = (unsigned)parsed;
        } else if (argc != 2) {
            usage(argv[0]);
            return EXIT_FAILURE;
        }

        return start_session(
                   duration,
                   one_opp_only) == 0
            ? EXIT_SUCCESS
            : EXIT_FAILURE;
    }

    if (argc == 2 && strcmp(argv[1], "stop") == 0)
        return clear_session() == 0
            ? EXIT_SUCCESS
            : EXIT_FAILURE;

    if (argc == 2 && strcmp(argv[1], "status") == 0)
        return print_status() == 0
            ? EXIT_SUCCESS
            : EXIT_FAILURE;

    if (argc == 2 &&
        strcmp(argv[1], "boot-resume") == 0)
        return boot_resume() == 0
            ? EXIT_SUCCESS
            : EXIT_FAILURE;

    usage(argv[0]);
    return EXIT_FAILURE;
}
