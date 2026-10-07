#include "flip_optimizer_guard.h"
#include "flip_optimizer_pmu_token.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    char trial_id[FLIP_OPTIMIZER_GUARD_TRIAL_ID_MAX + 1];
    unsigned epoch;
} active_trial_t;

static unsigned resolve_epoch(unsigned epoch)
{
    if (epoch)
        return epoch;

    time_t now = time(NULL);
    if (now <= 0 || (unsigned long long)now > 0xffffffffULL)
        return 1;

    return (unsigned)now;
}

static int ensure_root(void)
{
    if (mkdir(FLIP_OPTIMIZER_GUARD_ROOT, 0755) == 0 ||
        errno == EEXIST)
        return 0;

    return -1;
}

static int fsync_root(void)
{
    int fd = open(FLIP_OPTIMIZER_GUARD_ROOT, O_RDONLY);
    if (fd < 0)
        return -1;

    int rc = fsync(fd);
    int saved_errno = errno;
    close(fd);
    errno = saved_errno;
    return rc;
}

static int valid_trial_id(const char *trial_id)
{
    if (!trial_id || !trial_id[0])
        return 0;

    size_t length = strlen(trial_id);
    if (length > FLIP_OPTIMIZER_GUARD_TRIAL_ID_MAX)
        return 0;

    for (size_t i = 0; i < length; ++i) {
        unsigned char ch = (unsigned char)trial_id[i];
        if (!(isalnum(ch) || ch == '-' || ch == '_' || ch == '.'))
            return 0;
    }

    return 1;
}

static int parse_unsigned(const char *text, unsigned *value)
{
    if (!text || !text[0] || !value)
        return -1;

    errno = 0;
    char *end = NULL;
    unsigned long parsed = strtoul(text, &end, 10);

    if (errno || end == text || *end != '\0' || parsed > 0xffffffffUL)
        return -1;

    *value = (unsigned)parsed;
    return 0;
}

static int atomic_write(
    const char *path,
    const char *contents)
{
    if (ensure_root() < 0)
        return -1;

    char tmp[512];
    int n = snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    if (n < 0 || (size_t)n >= sizeof(tmp))
        return -1;

    FILE *f = fopen(tmp, "w");
    if (!f)
        return -1;

    size_t length = strlen(contents);
    int failed = 0;

    if (fwrite(contents, 1, length, f) != length)
        failed = 1;
    if (!failed && fflush(f) != 0)
        failed = 1;
    if (!failed && fsync(fileno(f)) != 0)
        failed = 1;
    if (fclose(f) != 0)
        failed = 1;

    if (failed) {
        unlink(tmp);
        return -1;
    }

    if (rename(tmp, path) != 0) {
        unlink(tmp);
        return -1;
    }

    return fsync_root();
}

static int write_event(
    const char *action,
    const char *trial_id,
    unsigned epoch)
{
    char contents[512];

    int n = snprintf(
        contents,
        sizeof(contents),
        "version=%d\n"
        "action=%s\n"
        "trial_id=%s\n"
        "epoch=%u\n",
        FLIP_OPTIMIZER_GUARD_VERSION,
        action,
        trial_id && trial_id[0] ? trial_id : "unknown",
        resolve_epoch(epoch));

    if (n < 0 || (size_t)n >= sizeof(contents))
        return -1;

    return atomic_write(
        FLIP_OPTIMIZER_GUARD_EVENT_FILE,
        contents);
}

static int write_stock_only(
    const char *trial_id,
    unsigned epoch)
{
    char contents[512];

    int n = snprintf(
        contents,
        sizeof(contents),
        "version=%d\n"
        "reason=trial_rollback\n"
        "trial_id=%s\n"
        "epoch=%u\n",
        FLIP_OPTIMIZER_GUARD_VERSION,
        trial_id && trial_id[0] ? trial_id : "unknown",
        resolve_epoch(epoch));

    if (n < 0 || (size_t)n >= sizeof(contents))
        return -1;

    return atomic_write(
        FLIP_OPTIMIZER_GUARD_STOCK_ONLY_FILE,
        contents);
}

static int read_active(active_trial_t *trial)
{
    if (!trial)
        return -1;

    memset(trial, 0, sizeof(*trial));

    FILE *f = fopen(FLIP_OPTIMIZER_GUARD_ACTIVE_FILE, "r");
    if (!f)
        return errno == ENOENT ? 0 : -1;

    int version = 0;
    int have_version = 0;
    int have_id = 0;
    int have_epoch = 0;
    char line[256];

    while (fgets(line, sizeof(line), f)) {
        char *end = strpbrk(line, "\r\n");
        if (end)
            *end = '\0';

        char *eq = strchr(line, '=');
        if (!eq)
            continue;

        *eq++ = '\0';

        if (strcmp(line, "version") == 0) {
            unsigned parsed = 0;
            if (parse_unsigned(eq, &parsed) < 0) {
                fclose(f);
                return -2;
            }
            version = (int)parsed;
            have_version = 1;
        } else if (strcmp(line, "trial_id") == 0) {
            if (!valid_trial_id(eq)) {
                fclose(f);
                return -2;
            }
            strcpy(trial->trial_id, eq);
            have_id = 1;
        } else if (strcmp(line, "armed_epoch") == 0) {
            if (parse_unsigned(eq, &trial->epoch) < 0) {
                fclose(f);
                return -2;
            }
            have_epoch = 1;
        }
    }

    if (fclose(f) != 0)
        return -1;

    if (!have_version ||
        version != FLIP_OPTIMIZER_GUARD_VERSION ||
        !have_id ||
        !have_epoch)
        return -2;

    return 1;
}

static int read_event(
    char *action,
    size_t action_size,
    char *trial_id,
    size_t trial_id_size,
    unsigned *epoch)
{
    FILE *f = fopen(FLIP_OPTIMIZER_GUARD_EVENT_FILE, "r");
    if (!f)
        return errno == ENOENT ? 0 : -1;

    int version = 0;
    int have_version = 0;
    int have_action = 0;
    int have_id = 0;
    int have_epoch = 0;
    char line[256];

    while (fgets(line, sizeof(line), f)) {
        char *end = strpbrk(line, "\r\n");
        if (end)
            *end = '\0';

        char *eq = strchr(line, '=');
        if (!eq)
            continue;

        *eq++ = '\0';

        if (strcmp(line, "version") == 0) {
            unsigned parsed = 0;
            if (parse_unsigned(eq, &parsed) < 0) {
                fclose(f);
                return -1;
            }
            version = (int)parsed;
            have_version = 1;
        } else if (strcmp(line, "action") == 0) {
            if (!eq[0] || strlen(eq) >= action_size) {
                fclose(f);
                return -1;
            }
            strcpy(action, eq);
            have_action = 1;
        } else if (strcmp(line, "trial_id") == 0) {
            if (!eq[0] || strlen(eq) >= trial_id_size) {
                fclose(f);
                return -1;
            }
            strcpy(trial_id, eq);
            have_id = 1;
        } else if (strcmp(line, "epoch") == 0) {
            if (parse_unsigned(eq, epoch) < 0) {
                fclose(f);
                return -1;
            }
            have_epoch = 1;
        }
    }

    if (fclose(f) != 0)
        return -1;

    if (!have_version ||
        version != FLIP_OPTIMIZER_GUARD_VERSION ||
        !have_action ||
        !have_id ||
        !have_epoch)
        return -1;

    return 1;
}

int flip_optimizer_guard_arm(
    const char *trial_id,
    unsigned epoch)
{
    if (!valid_trial_id(trial_id))
        return -1;

    if (access(FLIP_OPTIMIZER_GUARD_ACTIVE_FILE, F_OK) == 0 ||
        access(FLIP_OPTIMIZER_GUARD_STOCK_ONLY_FILE, F_OK) == 0)
        return -1;

    if (errno != ENOENT)
        return -1;

    char contents[512];

    int n = snprintf(
        contents,
        sizeof(contents),
        "version=%d\n"
        "trial_id=%s\n"
        "armed_epoch=%u\n",
        FLIP_OPTIMIZER_GUARD_VERSION,
        trial_id,
        resolve_epoch(epoch));

    if (n < 0 || (size_t)n >= sizeof(contents))
        return -1;

    if (atomic_write(
            FLIP_OPTIMIZER_GUARD_ACTIVE_FILE,
            contents) < 0)
        return -1;

    if (write_event(
            "armed",
            trial_id,
            epoch) < 0) {
        unlink(FLIP_OPTIMIZER_GUARD_ACTIVE_FILE);
        fsync_root();
        return -1;
    }

    int pmu_rc =
        flip_optimizer_pmu_token_write_trial(trial_id);

    if (strncmp(trial_id, "cpu-", 4) == 0 &&
        pmu_rc != 1) {
        unlink(FLIP_OPTIMIZER_GUARD_ACTIVE_FILE);
        fsync_root();
        (void)write_event(
            "cancelled",
            trial_id,
            epoch);
        return -1;
    }

    return 0;
}

int flip_optimizer_guard_mark_entering(
    const char *trial_id)
{
    if (!valid_trial_id(trial_id))
        return -1;

    active_trial_t active;
    int rc = read_active(&active);

    if (rc != 1 ||
        strcmp(active.trial_id, trial_id) != 0)
        return -1;

    if (strncmp(trial_id, "cpu-", 4) != 0)
        return 0;

    return flip_optimizer_pmu_token_write_trial_phase(
               trial_id,
               FLIP_OPTIMIZER_PMU_PHASE_ENTERING) == 1
        ? 0
        : -1;
}

int flip_optimizer_guard_confirm(
    const char *trial_id,
    unsigned epoch)
{
    if (!valid_trial_id(trial_id))
        return -1;

    active_trial_t active;
    int rc = read_active(&active);

    if (rc != 1 || strcmp(active.trial_id, trial_id) != 0)
        return -1;

    if (write_event(
            "confirmed",
            trial_id,
            epoch) < 0)
        return -1;

    if (unlink(FLIP_OPTIMIZER_GUARD_ACTIVE_FILE) != 0)
        return -1;

    if (fsync_root() < 0)
        return -1;

    if (unlink(FLIP_OPTIMIZER_GUARD_STOCK_ONLY_FILE) != 0 &&
        errno != ENOENT)
        return -1;

    if (fsync_root() < 0)
        return -1;

    return flip_optimizer_pmu_token_clear();
}

int flip_optimizer_guard_fail(
    const char *trial_id,
    unsigned epoch)
{
    if (!valid_trial_id(trial_id))
        return -1;

    active_trial_t active;
    int rc = read_active(&active);

    if (rc != 1 || strcmp(active.trial_id, trial_id) != 0)
        return -1;

    if (write_event(
            "failed",
            trial_id,
            epoch) < 0)
        return -1;

    if (unlink(FLIP_OPTIMIZER_GUARD_ACTIVE_FILE) != 0)
        return -1;

    if (fsync_root() < 0)
        return -1;

    return flip_optimizer_pmu_token_clear();
}

int flip_optimizer_guard_cancel(
    const char *trial_id,
    unsigned epoch)
{
    if (!valid_trial_id(trial_id))
        return -1;

    active_trial_t active;
    int rc = read_active(&active);

    if (rc != 1 || strcmp(active.trial_id, trial_id) != 0)
        return -1;

    if (write_event(
            "cancelled",
            trial_id,
            epoch) < 0)
        return -1;

    if (unlink(FLIP_OPTIMIZER_GUARD_ACTIVE_FILE) != 0)
        return -1;

    if (fsync_root() < 0)
        return -1;

    return flip_optimizer_pmu_token_clear();
}

static int recover_armed_event_trial(
    active_trial_t *trial)
{
    if (!trial)
        return -1;

    char action[64];
    char trial_id[FLIP_OPTIMIZER_GUARD_TRIAL_ID_MAX + 1];
    unsigned epoch = 0;

    int rc = read_event(
        action,
        sizeof(action),
        trial_id,
        sizeof(trial_id),
        &epoch);

    if (rc != 1 ||
        strcmp(action, "armed") != 0 ||
        !valid_trial_id(trial_id))
        return 0;

    memset(trial, 0, sizeof(*trial));
    strcpy(trial->trial_id, trial_id);
    trial->epoch = epoch;
    return 1;
}

int flip_optimizer_guard_boot_inspect(
    flip_optimizer_guard_boot_inspection_t *inspection)
{
    if (!inspection)
        return -1;

    memset(inspection, 0, sizeof(*inspection));

    char pmu_trial[FLIP_OPTIMIZER_GUARD_TRIAL_ID_MAX + 1];
    flip_optimizer_pmu_phase_t pmu_phase =
        FLIP_OPTIMIZER_PMU_PHASE_PREPARED;
    int pmu_rc = flip_optimizer_pmu_token_read_trial_phase(
        pmu_trial,
        sizeof(pmu_trial),
        &pmu_phase);

    if (pmu_rc == -1) {
        inspection->state =
            FLIP_OPTIMIZER_GUARD_BOOT_IO_ERROR;
        return 0;
    }

    if (pmu_rc == -2) {
        inspection->state =
            FLIP_OPTIMIZER_GUARD_BOOT_CORRUPT;
        return 0;
    }

    active_trial_t active;
    int rc = read_active(&active);

    if (pmu_rc == 1) {
        int active_matches =
            rc == 1 &&
            strcmp(active.trial_id, pmu_trial) == 0;

        if (pmu_phase ==
            FLIP_OPTIMIZER_PMU_PHASE_PREPARED) {
            inspection->state =
                active_matches
                    ? FLIP_OPTIMIZER_GUARD_BOOT_PREPARED
                    : FLIP_OPTIMIZER_GUARD_BOOT_CORRUPT;
        } else {
            inspection->state =
                active_matches
                    ? FLIP_OPTIMIZER_GUARD_BOOT_UNCONFIRMED
                    : FLIP_OPTIMIZER_GUARD_BOOT_CORRUPT;
        }

        strcpy(inspection->trial_id, pmu_trial);
        inspection->armed_epoch =
            rc == 1 ? active.epoch : 0;
        return 0;
    }

    if (rc == 0) {
        inspection->state = FLIP_OPTIMIZER_GUARD_BOOT_CLEAN;
        return 0;
    }

    if (rc == 1) {
        inspection->state =
            FLIP_OPTIMIZER_GUARD_BOOT_UNCONFIRMED;
        strcpy(inspection->trial_id, active.trial_id);
        inspection->armed_epoch = active.epoch;
        return 0;
    }

    if (rc == -2) {
        active_trial_t recovered;
        if (recover_armed_event_trial(&recovered) == 1) {
            inspection->state =
                FLIP_OPTIMIZER_GUARD_BOOT_CORRUPT;
            strcpy(inspection->trial_id, recovered.trial_id);
            inspection->armed_epoch = recovered.epoch;
            return 0;
        }
    }

    inspection->state =
        rc == -2
            ? FLIP_OPTIMIZER_GUARD_BOOT_CORRUPT
            : FLIP_OPTIMIZER_GUARD_BOOT_IO_ERROR;
    return 0;
}

int flip_optimizer_guard_boot_check(
    unsigned epoch,
    char *rolled_back_trial,
    size_t rolled_back_trial_size)
{
    if (rolled_back_trial && rolled_back_trial_size)
        rolled_back_trial[0] = '\0';

    char pmu_trial[FLIP_OPTIMIZER_GUARD_TRIAL_ID_MAX + 1];
    flip_optimizer_pmu_phase_t pmu_phase =
        FLIP_OPTIMIZER_PMU_PHASE_PREPARED;
    int pmu_rc = flip_optimizer_pmu_token_read_trial_phase(
        pmu_trial,
        sizeof(pmu_trial),
        &pmu_phase);

    active_trial_t active;
    int rc = read_active(&active);

    if (rc == 0 && pmu_rc == 0)
        return 0;

    if (pmu_rc == 1 &&
        pmu_phase == FLIP_OPTIMIZER_PMU_PHASE_PREPARED) {
        int conflicting_active =
            rc == 1 &&
            strcmp(active.trial_id, pmu_trial) != 0;

        if (rc == -1)
            return -1;

        if (!conflicting_active) {
            if (write_event(
                    "cancelled_unentered",
                    pmu_trial,
                    epoch) < 0)
                return -1;

            if (unlink(
                    FLIP_OPTIMIZER_GUARD_ACTIVE_FILE) != 0 &&
                errno != ENOENT)
                return -1;

            if (fsync_root() < 0 ||
                flip_optimizer_pmu_token_clear() < 0)
                return -1;

            if (rolled_back_trial &&
                rolled_back_trial_size) {
                int n = snprintf(
                    rolled_back_trial,
                    rolled_back_trial_size,
                    "%s",
                    pmu_trial);
                if (n < 0 ||
                    (size_t)n >=
                        rolled_back_trial_size)
                    return -1;
            }

            return 0;
        }
    }

    active_trial_t recovered;
    int recovered_rc =
        rc == -2
            ? recover_armed_event_trial(&recovered)
            : 0;

    const char *trial_id = "unknown";
    int corrupt = 1;

    if (pmu_rc == 1) {
        trial_id = pmu_trial;
        corrupt =
            !(rc == 1 &&
              strcmp(active.trial_id, pmu_trial) == 0);
    } else if (rc == 1) {
        trial_id = active.trial_id;
        corrupt = 0;
    } else if (recovered_rc == 1) {
        trial_id = recovered.trial_id;
    }

    const char *action =
        corrupt ? "rollback_corrupt" : "rolled_back";

    if (write_stock_only(trial_id, epoch) < 0 ||
        write_event(action, trial_id, epoch) < 0)
        return -1;

    if (unlink(FLIP_OPTIMIZER_GUARD_ACTIVE_FILE) != 0 &&
        errno != ENOENT)
        return -1;

    if (fsync_root() < 0)
        return -1;

    if (flip_optimizer_pmu_token_clear() < 0)
        return -1;

    if (rolled_back_trial && rolled_back_trial_size) {
        int n = snprintf(
            rolled_back_trial,
            rolled_back_trial_size,
            "%s",
            trial_id);
        if (n < 0 || (size_t)n >= rolled_back_trial_size)
            return -1;
    }

    return 1;
}

int flip_optimizer_guard_clear_stock_only(void)
{
    if (unlink(FLIP_OPTIMIZER_GUARD_STOCK_ONLY_FILE) != 0 &&
        errno != ENOENT)
        return -1;

    return ensure_root() == 0 ? fsync_root() : -1;
}

int flip_optimizer_guard_status(
    flip_optimizer_guard_status_t *status)
{
    if (!status)
        return -1;

    memset(status, 0, sizeof(*status));

    active_trial_t active;
    int active_rc = read_active(&active);

    if (active_rc < 0)
        return -1;

    if (active_rc == 1) {
        status->active = 1;
        strcpy(status->trial_id, active.trial_id);
        status->armed_epoch = active.epoch;
    }

    status->stock_only =
        access(FLIP_OPTIMIZER_GUARD_STOCK_ONLY_FILE, F_OK) == 0;

    if (!status->stock_only && errno != ENOENT)
        return -1;

    int event_rc = read_event(
        status->last_action,
        sizeof(status->last_action),
        status->last_trial_id,
        sizeof(status->last_trial_id),
        &status->last_epoch);

    if (event_rc < 0)
        return -1;

    return 0;
}
