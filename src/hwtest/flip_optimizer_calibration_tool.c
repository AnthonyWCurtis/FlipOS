#include "flip_optimizer_calibration.h"
#include "flip_optimizer_calibration_session.h"
#include "flip_optimizer_guard.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define CPUINFO_PATH "/proc/cpuinfo"
#define DT_SERIAL_PATH "/sys/firmware/devicetree/base/serial-number"
#define MODEL_PATH "/proc/device-tree/model"
#define REGULATOR_ROOT "/sys/class/regulator"
#define CPU_OPP_ROOT "/sys/kernel/debug/opp/cpu0"

#ifndef FLIP_OPTIMIZER_EXPERIMENTAL_BUILD
#define FLIP_OPTIMIZER_EXPERIMENTAL_BUILD 0
#endif

#define SNAPSHOT_MARKER \
    FLIP_OPTIMIZER_CALIBRATION_ROOT "/rollback-snapshot"
#define SNAPSHOT_CALIBRATION \
    FLIP_OPTIMIZER_CALIBRATION_ROOT "/cpu-calibration.previous"
#define SNAPSHOT_RESULT \
    FLIP_OPTIMIZER_CALIBRATION_ROOT "/cpu-result.previous"
#define SNAPSHOT_REPORT \
    FLIP_OPTIMIZER_CALIBRATION_ROOT "/cpu-report.previous"
#define OPTIMIZER_HISTORY_FILE \
    FLIP_OPTIMIZER_CALIBRATION_ROOT "/cpu-history"

static int read_text(
    const char *path,
    char *buffer,
    size_t size)
{
    if (!path || !buffer || size < 2)
        return -1;

    FILE *f = fopen(path, "rb");
    if (!f)
        return -1;

    size_t n = fread(buffer, 1, size - 1, f);
    int failed = ferror(f);
    fclose(f);

    if (failed || n == 0)
        return -1;

    buffer[n] = '\0';

    while (n > 0 &&
           (buffer[n - 1] == '\0' ||
            buffer[n - 1] == '\n' ||
            buffer[n - 1] == '\r' ||
            buffer[n - 1] == ' ' ||
            buffer[n - 1] == '\t')) {
        buffer[--n] = '\0';
    }

    return n ? 0 : -1;
}

static int read_unsigned(
    const char *path,
    unsigned *value)
{
    char buffer[64];

    if (read_text(path, buffer, sizeof(buffer)) < 0)
        return -1;

    errno = 0;
    char *end = NULL;
    unsigned long parsed = strtoul(buffer, &end, 10);

    if (errno || end == buffer || *end != '\0' ||
        parsed > 0xffffffffUL)
        return -1;

    *value = (unsigned)parsed;
    return 0;
}

static int read_device_id(
    char *device_id,
    size_t size)
{
    /* Rockchip's /proc/cpuinfo Serial is not stable on this firmware; it can
     * change across otherwise normal boots. The DT serial-number is derived
     * from the board/SoC identity and remains stable, so prefer it for binding
     * a device-specific voltage profile. Keep cpuinfo only as a legacy
     * fallback for boards whose DT lacks serial-number. */
    if (device_id && size >= 2) {
        FILE *dt = fopen(DT_SERIAL_PATH, "rb");
        if (dt) {
            size_t n = fread(device_id, 1, size - 1, dt);
            fclose(dt);
            if (n > 0) {
                device_id[n] = '\0';
                for (size_t i = 0; i < n; ++i) {
                    if (device_id[i] == '\0' ||
                        device_id[i] == '\r' ||
                        device_id[i] == '\n' ||
                        device_id[i] == ' ' ||
                        device_id[i] == '\t') {
                        device_id[i] = '\0';
                        break;
                    }
                }
                if (device_id[0])
                    return 0;
            }
        }
    }

    FILE *f = fopen(CPUINFO_PATH, "r");
    if (!f)
        return -1;

    char line[256];
    int found = 0;

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "Serial", 6) != 0)
            continue;

        char *colon = strchr(line, ':');
        if (!colon)
            continue;

        char *value = colon + 1;
        while (*value == ' ' || *value == '\t')
            ++value;

        char *end = strpbrk(value, "\r\n");
        if (end)
            *end = '\0';

        if (!value[0] || strlen(value) >= size)
            break;

        strcpy(device_id, value);
        found = 1;
        break;
    }

    fclose(f);
    return found ? 0 : -1;
}

static int read_regulator_floor(unsigned *floor_uv)
{
    DIR *root = opendir(REGULATOR_ROOT);
    if (!root)
        return -1;

    struct dirent *entry;
    int rc = -1;

    while ((entry = readdir(root)) != NULL) {
        if (entry->d_name[0] == '.')
            continue;

        char path[512];
        char name[128];

        int n = snprintf(
            path,
            sizeof(path),
            "%s/%s/name",
            REGULATOR_ROOT,
            entry->d_name);

        if (n < 0 || (size_t)n >= sizeof(path) ||
            read_text(path, name, sizeof(name)) < 0 ||
            strcmp(name, "vdd_cpu") != 0)
            continue;

        n = snprintf(
            path,
            sizeof(path),
            "%s/%s/min_microvolts",
            REGULATOR_ROOT,
            entry->d_name);

        if (n < 0 || (size_t)n >= sizeof(path))
            break;

        rc = read_unsigned(path, floor_uv);
        break;
    }

    closedir(root);
    return rc;
}

static int compare_opps(
    const void *left,
    const void *right)
{
    const flip_optimizer_calibration_stock_opp_t *a =
        left;
    const flip_optimizer_calibration_stock_opp_t *b =
        right;

    if (a->frequency_khz < b->frequency_khz)
        return -1;
    if (a->frequency_khz > b->frequency_khz)
        return 1;
    return 0;
}

static int read_cpu_opps(
    unsigned floor_uv,
    flip_optimizer_calibration_stock_opp_t *opps,
    size_t capacity,
    size_t *opp_count)
{
    DIR *root = opendir(CPU_OPP_ROOT);
    if (!root)
        return -1;

    size_t count = 0;
    struct dirent *entry;

    while ((entry = readdir(root)) != NULL) {
        if (strncmp(entry->d_name, "opp:", 4) != 0)
            continue;

        if (count >= capacity) {
            closedir(root);
            return -1;
        }

        char base[512];
        char path[640];
        char available[16];

        int n = snprintf(
            base,
            sizeof(base),
            "%s/%s",
            CPU_OPP_ROOT,
            entry->d_name);

        if (n < 0 || (size_t)n >= sizeof(base))
            continue;

        n = snprintf(
            path,
            sizeof(path),
            "%s/available",
            base);

        if (n < 0 || (size_t)n >= sizeof(path) ||
            read_text(
                path,
                available,
                sizeof(available)) < 0 ||
            strcmp(available, "Y") != 0)
            continue;

        unsigned rate_hz = 0;
        unsigned stock_uv = 0;

        n = snprintf(
            path,
            sizeof(path),
            "%s/rate_hz",
            base);

        if (n < 0 || (size_t)n >= sizeof(path) ||
            read_unsigned(path, &rate_hz) < 0)
            continue;

        n = snprintf(
            path,
            sizeof(path),
            "%s/supply-0/u_volt_target",
            base);

        if (n < 0 || (size_t)n >= sizeof(path) ||
            read_unsigned(path, &stock_uv) < 0)
            continue;

        if (rate_hz % 1000 != 0 ||
            stock_uv < floor_uv)
            continue;

        opps[count].frequency_khz = rate_hz / 1000;
        opps[count].stock_uv = stock_uv;
        opps[count].floor_uv = floor_uv;
        count++;
    }

    closedir(root);

    if (!count)
        return -1;

    qsort(
        opps,
        count,
        sizeof(opps[0]),
        compare_opps);

    *opp_count = count;
    return 0;
}

static int probe(
    char *device_id,
    size_t device_id_size,
    char *model,
    size_t model_size,
    unsigned *floor_uv,
    flip_optimizer_calibration_stock_opp_t *opps,
    size_t capacity,
    size_t *opp_count)
{
    if (read_device_id(
            device_id,
            device_id_size) < 0 ||
        read_text(
            MODEL_PATH,
            model,
            model_size) < 0 ||
        read_regulator_floor(floor_uv) < 0)
        return -1;

    if (!strstr(model, "RK3566"))
        return -1;

    /*
     * The regulator's electrical minimum is lower than the RK3566 CPU's
     * specified operating minimum. Never use the regulator floor as the
     * silicon calibration floor.
     */
    if (*floor_uv <
        FLIP_OPTIMIZER_CALIBRATION_RK3566_FLOOR_UV)
        *floor_uv =
            FLIP_OPTIMIZER_CALIBRATION_RK3566_FLOOR_UV;

    if (read_cpu_opps(
            *floor_uv,
            opps,
            capacity,
            opp_count) < 0)
        return -1;

    return 0;
}

static void print_plan(
    const flip_optimizer_calibration_t *calibration)
{
    printf("device_id=%s\n", calibration->device_id);
    printf("model=%s\n", calibration->model);
    printf(
        "voltage_step_uv=%u\n",
        calibration->voltage_step_uv);
    printf(
        "guardband_uv=%u\n",
        calibration->guardband_uv);
    printf("opp_count=%zu\n", calibration->opp_count);

    for (size_t reverse = calibration->opp_count;
         reverse > 0;
         --reverse) {
        size_t i = reverse - 1;
        const flip_optimizer_calibration_opp_t *opp =
            &calibration->opps[i];

        unsigned first_candidate = 0;

        if (!opp->complete) {
            if (opp->highest_unstable_uv) {
                unsigned gap =
                    opp->lowest_stable_uv -
                    opp->highest_unstable_uv;

                if (gap > calibration->voltage_step_uv) {
                    unsigned midpoint_steps =
                        (gap / calibration->voltage_step_uv) / 2U;

                    if (midpoint_steps == 0)
                        midpoint_steps = 1;

                    first_candidate =
                        opp->highest_unstable_uv +
                        midpoint_steps *
                            calibration->voltage_step_uv;
                }
            } else if (opp->lowest_stable_uv >=
                       opp->floor_uv +
                           calibration->voltage_step_uv) {
                unsigned coarse_step =
                    opp->tests_run == 0
                        ? calibration->voltage_step_uv
                        : calibration->voltage_step_uv * 2U;
                unsigned available =
                    opp->lowest_stable_uv -
                    opp->floor_uv;

                if (coarse_step > available)
                    coarse_step =
                        (available /
                         calibration->voltage_step_uv) *
                        calibration->voltage_step_uv;

                if (coarse_step == 0)
                    coarse_step =
                        calibration->voltage_step_uv;

                first_candidate =
                    opp->lowest_stable_uv - coarse_step;
            }
        }

        printf(
            "opp%zu=%uKHz stock=%uuV floor=%uuV "
            "first_candidate=%uuV\n",
            i,
            opp->frequency_khz,
            opp->stock_uv,
            opp->floor_uv,
            first_candidate);
    }
}

static int append_history_line(const char *line)
{
    if (!line || !line[0])
        return -1;

    FILE *f = fopen(OPTIMIZER_HISTORY_FILE, "a");
    if (!f)
        return -1;

    int failed = fprintf(f, "%s\n", line) < 0;
    if (!failed && fflush(f) != 0)
        failed = 1;
    if (!failed && fsync(fileno(f)) != 0)
        failed = 1;
    if (fclose(f) != 0)
        failed = 1;

    return failed ? -1 : 0;
}

static int append_history_unstable(
    const flip_optimizer_calibration_t *calibration,
    size_t index,
    unsigned voltage_uv,
    const char *source)
{
    if (!calibration ||
        index >= calibration->opp_count ||
        !voltage_uv ||
        !source || !source[0])
        return -1;

    time_t now = time(NULL);
    unsigned epoch =
        now > 0 && (unsigned long long)now <= 0xffffffffULL
            ? (unsigned)now
            : 0;

    char line[512];
    int n = snprintf(
        line,
        sizeof(line),
        "event=unstable device_id=%s frequency_khz=%u voltage_uv=%u "
        "epoch=%u source=%s",
        calibration->device_id,
        calibration->opps[index].frequency_khz,
        voltage_uv,
        epoch,
        source);

    if (n < 0 || (size_t)n >= sizeof(line))
        return -1;

    return append_history_line(line);
}

static int append_history_boundary(
    const flip_optimizer_calibration_t *calibration,
    size_t index,
    unsigned epoch)
{
    if (!calibration || index >= calibration->opp_count)
        return -1;

    const flip_optimizer_calibration_opp_t *opp =
        &calibration->opps[index];

    char line[640];
    int n = snprintf(
        line,
        sizeof(line),
        "event=boundary device_id=%s frequency_khz=%u "
        "lowest_stable_uv=%u highest_unstable_uv=%u raw_final_uv=%u "
        "tests=%u epoch=%u",
        calibration->device_id,
        opp->frequency_khz,
        opp->lowest_stable_uv,
        opp->highest_unstable_uv,
        opp->raw_final_uv,
        opp->tests_run,
        epoch);

    if (n < 0 || (size_t)n >= sizeof(line))
        return -1;

    return append_history_line(line);
}

static unsigned history_highest_unstable_uv(
    const char *device_id,
    unsigned frequency_khz)
{
    FILE *f = fopen(OPTIMIZER_HISTORY_FILE, "r");
    if (!f)
        return 0;

    unsigned highest = 0;
    char line[768];

    while (fgets(line, sizeof(line), f)) {
        char event[32] = {0};
        char recorded_device[FLIP_OPTIMIZER_DEVICE_ID_MAX] = {0};
        unsigned recorded_khz = 0;
        unsigned voltage_uv = 0;

        if (sscanf(
                line,
                "event=%31s device_id=%63s frequency_khz=%u voltage_uv=%u",
                event,
                recorded_device,
                &recorded_khz,
                &voltage_uv) != 4)
            continue;

        if (strcmp(event, "unstable") != 0 ||
            strcmp(recorded_device, device_id) != 0 ||
            recorded_khz != frequency_khz)
            continue;

        if (voltage_uv > highest)
            highest = voltage_uv;
    }

    fclose(f);
    return highest;
}

static int apply_failure_history(
    flip_optimizer_calibration_t *calibration)
{
    if (!calibration)
        return -1;

    for (size_t i = 0; i < calibration->opp_count; ++i) {
        flip_optimizer_calibration_opp_t *opp =
            &calibration->opps[i];

        if (opp->complete)
            continue;

        unsigned known_failed_uv =
            history_highest_unstable_uv(
                calibration->device_id,
                opp->frequency_khz);

        if (!known_failed_uv ||
            known_failed_uv < opp->floor_uv ||
            known_failed_uv >= opp->stock_uv)
            continue;

        opp->highest_unstable_uv = known_failed_uv;

        printf(
            "history_seeded frequency_khz=%u highest_unstable_uv=%u\n",
            opp->frequency_khz,
            known_failed_uv);
    }

    return flip_optimizer_calibration_validate(calibration);
}

static int make_live_calibration(
    flip_optimizer_calibration_t *calibration)
{
    char device_id[FLIP_OPTIMIZER_DEVICE_ID_MAX];
    char model[FLIP_OPTIMIZER_MODEL_MAX];
    unsigned floor_uv = 0;
    flip_optimizer_calibration_stock_opp_t
        opps[FLIP_OPTIMIZER_CALIBRATION_MAX_OPPS];
    size_t opp_count = 0;

    if (probe(
            device_id,
            sizeof(device_id),
            model,
            sizeof(model),
            &floor_uv,
            opps,
            FLIP_OPTIMIZER_CALIBRATION_MAX_OPPS,
            &opp_count) < 0)
        return -1;

    if (flip_optimizer_calibration_init(
            calibration,
            device_id,
            model,
            opps,
            opp_count,
            FLIP_OPTIMIZER_CALIBRATION_DEFAULT_STEP_UV,
            FLIP_OPTIMIZER_CALIBRATION_DEFAULT_GUARDBAND_UV) < 0)
        return -1;

    return flip_optimizer_calibration_preserve_stock_below(
        calibration,
        FLIP_OPTIMIZER_CALIBRATION_RK3566_MIN_TUNABLE_KHZ);
}

#if FLIP_OPTIMIZER_EXPERIMENTAL_BUILD
static int make_first_experimental_calibration(
    flip_optimizer_calibration_t *experimental)
{
    if (!experimental)
        return -1;

    flip_optimizer_calibration_t production;

    if (flip_optimizer_calibration_load_file(
            &production,
            FLIP_OPTIMIZER_PRODUCTION_CALIBRATION_FILE) < 0 ||
        !flip_optimizer_calibration_is_complete(
            &production) ||
        make_live_calibration(experimental) < 0)
        return -1;

    if (strcmp(production.device_id, experimental->device_id) != 0 ||
        strcmp(production.model, experimental->model) != 0 ||
        production.opp_count != experimental->opp_count)
        return -1;

    int armed_408 = 0;

    for (size_t i = 0; i < experimental->opp_count; ++i) {
        const flip_optimizer_calibration_opp_t *source =
            &production.opps[i];
        flip_optimizer_calibration_opp_t *target =
            &experimental->opps[i];

        if (source->frequency_khz != target->frequency_khz ||
            source->stock_uv != target->stock_uv ||
            !source->complete ||
            !source->raw_final_uv)
            return -1;

        target->lowest_stable_uv = source->lowest_stable_uv;
        target->highest_unstable_uv = source->highest_unstable_uv;
        target->tests_run = source->tests_run;

        if (target->frequency_khz == 408000U) {
            if (source->lowest_stable_uv !=
                    FLIP_OPTIMIZER_CALIBRATION_RK3566_SAFE_MIN_UV ||
                source->highest_unstable_uv != 0 ||
                target->floor_uv > 800000U)
                return -1;

            target->highest_unstable_uv = 0;
            target->raw_final_uv = 0;
            target->tests_run = 0;
            target->complete = 0;
            armed_408 = 1;
        } else {
            target->raw_final_uv = source->raw_final_uv;
            target->complete = 1;
        }
    }

    if (!armed_408 ||
        flip_optimizer_calibration_validate(experimental) < 0)
        return -1;

    flip_optimizer_calibration_t preview = *experimental;
    size_t preview_index = 0;
    unsigned preview_uv = 0;

    if (flip_optimizer_calibration_prepare_next(
            &preview,
            &preview_index,
            &preview_uv) != 1 ||
        preview.opps[preview_index].frequency_khz != 408000U ||
        preview_uv != 800000U)
        return -1;

    return 0;
}
#endif

static int print_status(void)
{
    flip_optimizer_calibration_t calibration;
    flip_optimizer_guard_status_t guard;

    if (flip_optimizer_calibration_load(
            &calibration) < 0 ||
        flip_optimizer_guard_status(&guard) < 0)
        return -1;

    print_plan(&calibration);
    printf("pending=%d\n", calibration.pending);

    if (calibration.pending) {
        printf(
            "pending_index=%zu\n",
            calibration.pending_index);
        printf(
            "pending_uv=%u\n",
            calibration.pending_uv);
        printf(
            "pending_trial_id=%s\n",
            calibration.pending_trial_id);
    }

    size_t complete = 0;
    for (size_t i = 0; i < calibration.opp_count; ++i)
        complete += calibration.opps[i].complete ? 1 : 0;

    printf("complete_opps=%zu\n", complete);
    printf("guard_active=%d\n", guard.active);
    printf("guard_stock_only=%d\n", guard.stock_only);
    if (guard.active)
        printf("guard_trial_id=%s\n", guard.trial_id);
    if (guard.last_action[0])
        printf("guard_last_action=%s\n", guard.last_action);

    printf(
        "ready_for_apply=%d\n",
        flip_optimizer_calibration_session_ready_for_apply(
            &calibration));

    return 0;
}

static int copy_file_durable(
    const char *source,
    const char *destination)
{
    char temp[512];
    int n = snprintf(
        temp,
        sizeof(temp),
        "%s.tmp",
        destination);
    if (n < 0 || (size_t)n >= sizeof(temp))
        return -1;

    int in = open(source, O_RDONLY | O_CLOEXEC);
    if (in < 0)
        return -1;

    int out = open(
        temp,
        O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,
        0644);
    if (out < 0) {
        close(in);
        return -1;
    }

    char buffer[4096];
    int failed = 0;

    for (;;) {
        ssize_t got = read(in, buffer, sizeof(buffer));
        if (got == 0)
            break;
        if (got < 0) {
            if (errno == EINTR)
                continue;
            failed = 1;
            break;
        }

        char *cursor = buffer;
        ssize_t left = got;
        while (left > 0) {
            ssize_t wrote = write(out, cursor, (size_t)left);
            if (wrote < 0) {
                if (errno == EINTR)
                    continue;
                failed = 1;
                break;
            }
            cursor += wrote;
            left -= wrote;
        }

        if (failed)
            break;
    }

    if (!failed && fsync(out) < 0)
        failed = 1;

    if (close(in) < 0)
        failed = 1;
    if (close(out) < 0)
        failed = 1;

    if (failed) {
        (void)unlink(temp);
        return -1;
    }

    if (rename(temp, destination) < 0) {
        (void)unlink(temp);
        return -1;
    }

    return 0;
}

static int read_snapshot_marker(
    int *have_calibration,
    int *have_result,
    int *have_report);

static int snapshot_optimizer_state(void)
{
    flip_optimizer_guard_status_t guard;
    flip_optimizer_calibration_t calibration;

    /*
     * A clean public install has no optimizer directory yet. Snapshotting an
     * empty previous state is still valid and is required before the first
     * calibration can be initialized, so create the state root here rather
     * than making first-run optimization fail before init gets a chance.
     */
    if (mkdir(FLIP_OPTIMIZER_CALIBRATION_ROOT, 0755) < 0 &&
        errno != EEXIST)
        return -1;

    /*
     * Never replace an unresolved rollback snapshot. If a previous restore
     * failed, that snapshot is still the last known-good state and must
     * survive subsequent UI retries until it is restored or superseded by
     * a fully completed/published optimization.
     */
    if (access(SNAPSHOT_MARKER, F_OK) == 0) {
        int have_calibration = 0;
        int have_result = 0;
        int have_report = 0;

        if (read_snapshot_marker(
                &have_calibration,
                &have_result,
                &have_report) < 0)
            return -1;

        if ((have_calibration &&
             access(SNAPSHOT_CALIBRATION, F_OK) != 0) ||
            (have_result &&
             access(SNAPSHOT_RESULT, F_OK) != 0) ||
            (have_report &&
             access(SNAPSHOT_REPORT, F_OK) != 0))
            return -1;

        printf("snapshot_preserved=1\n");
        printf("snapshot_has_result=%d\n", have_result);
        return 0;
    }

    if (flip_optimizer_guard_status(&guard) < 0 ||
        guard.active || guard.stock_only)
        return -1;

    int have_calibration =
        access(FLIP_OPTIMIZER_CALIBRATION_FILE, F_OK) == 0;

    if (have_calibration &&
        (flip_optimizer_calibration_load(&calibration) < 0 ||
         calibration.pending))
        return -1;
    int have_result =
        access(FLIP_OPTIMIZER_RESULT_FILE, F_OK) == 0;
    int have_report =
        access(FLIP_OPTIMIZER_REPORT_FILE, F_OK) == 0;

    if (have_calibration &&
        copy_file_durable(
            FLIP_OPTIMIZER_CALIBRATION_FILE,
            SNAPSHOT_CALIBRATION) < 0)
        return -1;

    if (have_result &&
        copy_file_durable(
            FLIP_OPTIMIZER_RESULT_FILE,
            SNAPSHOT_RESULT) < 0)
        return -1;

    if (have_report &&
        copy_file_durable(
            FLIP_OPTIMIZER_REPORT_FILE,
            SNAPSHOT_REPORT) < 0)
        return -1;

    FILE *marker_file = fopen(SNAPSHOT_MARKER ".tmp", "w");
    if (!marker_file)
        return -1;

    int failed = fprintf(
        marker_file,
        "version=1\ncalibration=%d\nresult=%d\nreport=%d\n",
        have_calibration,
        have_result,
        have_report) < 0;

    if (fflush(marker_file) != 0 ||
        fsync(fileno(marker_file)) != 0 ||
        fclose(marker_file) != 0)
        failed = 1;

    if (failed ||
        rename(SNAPSHOT_MARKER ".tmp", SNAPSHOT_MARKER) < 0) {
        (void)unlink(SNAPSHOT_MARKER ".tmp");
        return -1;
    }

    printf("snapshot_created=1\n");
    printf("snapshot_has_result=%d\n", have_result);
    return 0;
}

static int read_snapshot_marker(
    int *have_calibration,
    int *have_result,
    int *have_report)
{
    FILE *f = fopen(SNAPSHOT_MARKER, "r");
    if (!f)
        return -1;

    int version = 0;
    int c = 0, r = 0, p = 0;
    char line[128];

    while (fgets(line, sizeof(line), f)) {
        (void)sscanf(line, "version=%d", &version);
        (void)sscanf(line, "calibration=%d", &c);
        (void)sscanf(line, "result=%d", &r);
        (void)sscanf(line, "report=%d", &p);
    }

    int failed = ferror(f);
    fclose(f);

    if (failed || version != 1 ||
        (c != 0 && c != 1) ||
        (r != 0 && r != 1) ||
        (p != 0 && p != 1))
        return -1;

    *have_calibration = c;
    *have_result = r;
    *have_report = p;
    return 0;
}

static int discard_optimizer_snapshot(void)
{
    const char *paths[] = {
        SNAPSHOT_CALIBRATION,
        SNAPSHOT_RESULT,
        SNAPSHOT_REPORT,
        SNAPSHOT_MARKER,
    };

    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
        if (unlink(paths[i]) < 0 && errno != ENOENT)
            return -1;
    }

    return 0;
}

static int restore_optimizer_snapshot(void)
{
    flip_optimizer_guard_status_t guard;
    int have_calibration = 0;
    int have_result = 0;
    int have_report = 0;

    if (flip_optimizer_guard_status(&guard) < 0 ||
        guard.active || guard.stock_only ||
        read_snapshot_marker(
            &have_calibration,
            &have_result,
            &have_report) < 0)
        return -1;

    if (have_calibration) {
        if (copy_file_durable(
                SNAPSHOT_CALIBRATION,
                FLIP_OPTIMIZER_CALIBRATION_FILE) < 0)
            return -1;
    } else if (unlink(FLIP_OPTIMIZER_CALIBRATION_FILE) < 0 &&
               errno != ENOENT) {
        return -1;
    }

    if (have_result) {
        if (copy_file_durable(
                SNAPSHOT_RESULT,
                FLIP_OPTIMIZER_RESULT_FILE) < 0)
            return -1;
    } else if (unlink(FLIP_OPTIMIZER_RESULT_FILE) < 0 &&
               errno != ENOENT) {
        return -1;
    }

    if (have_report) {
        if (copy_file_durable(
                SNAPSHOT_REPORT,
                FLIP_OPTIMIZER_REPORT_FILE) < 0)
            return -1;
    } else if (unlink(FLIP_OPTIMIZER_REPORT_FILE) < 0 &&
               errno != ENOENT) {
        return -1;
    }

    if (discard_optimizer_snapshot() < 0)
        return -1;

    printf("snapshot_restored=1\n");
    printf("restored_result=%d\n", have_result);
    return 0;
}

static int publish_completed_calibration(void)
{
    flip_optimizer_calibration_t calibration;
    flip_optimizer_result_t result;

    if (flip_optimizer_calibration_load(&calibration) < 0 ||
        !flip_optimizer_calibration_is_complete(&calibration))
        return -1;

    time_t now = time(NULL);
    if (now <= 0 || (unsigned long long)now > 0xffffffffULL)
        return -1;

    if (flip_optimizer_calibration_build_result(
            &calibration,
            (unsigned)now,
            &result) < 0 ||
        flip_optimizer_save(&result) < 0)
        return -1;

    FILE *report = fopen(FLIP_OPTIMIZER_REPORT_FILE, "w");
    if (!report)
        return -1;

    int failed = 0;
    failed |= fprintf(
        report,
        "FlipOS CPU Optimization Report\n"
        "device_id=%s\n"
        "model=%s\n"
        "calibrated_epoch=%u\n"
        "guardband_uv=%u\n"
        "opp_count=%zu\n\n",
        calibration.device_id,
        calibration.model,
        result.calibrated_epoch,
        calibration.guardband_uv,
        calibration.opp_count) < 0;

    for (size_t i = 0; i < calibration.opp_count; ++i) {
        const flip_optimizer_calibration_opp_t *source =
            &calibration.opps[i];
        const flip_optimizer_opp_t *published =
            &result.opps[i];

        unsigned reduction_uv =
            published->stock_uv - published->optimized_uv;

        failed |= fprintf(
            report,
            "opp%zu frequency_khz=%u stock_uv=%u optimized_uv=%u "
            "reduction_uv=%u tests=%u mode=%s "
            "lowest_stable_uv=%u highest_unstable_uv=%u\n",
            i,
            published->frequency_khz,
            published->stock_uv,
            published->optimized_uv,
            reduction_uv,
            source->tests_run,
            source->tests_run == 0
                ? "stock-preserved"
                : "calibrated",
            source->lowest_stable_uv,
            source->highest_unstable_uv) < 0;
    }

    if (fflush(report) != 0 ||
        fsync(fileno(report)) != 0 ||
        fclose(report) != 0)
        failed = 1;

    if (failed)
        return -1;

    for (size_t i = 0; i < calibration.opp_count; ++i) {
        if (append_history_boundary(
                &calibration,
                i,
                result.calibrated_epoch) < 0)
            fprintf(
                stderr,
                "optimizer history: boundary write failed frequency_khz=%u\n",
                calibration.opps[i].frequency_khz);
    }

    printf("published=1\n");
    printf("result=%s\n", FLIP_OPTIMIZER_RESULT_FILE);
    printf("report=%s\n", FLIP_OPTIMIZER_REPORT_FILE);
    return 0;
}

static int reset_calibration_state(int discard_snapshot)
{
    flip_optimizer_guard_status_t guard;

    if (flip_optimizer_guard_status(&guard) < 0 ||
        guard.active ||
        guard.stock_only)
        return -1;

    if (unlink(FLIP_OPTIMIZER_CALIBRATION_FILE) != 0 &&
        errno != ENOENT)
        return -1;

    if (unlink(FLIP_OPTIMIZER_RESULT_FILE) != 0 &&
        errno != ENOENT)
        return -1;

    if (unlink(FLIP_OPTIMIZER_REPORT_FILE) != 0 &&
        errno != ENOENT)
        return -1;

    if (discard_snapshot &&
        discard_optimizer_snapshot() < 0)
        return -1;

    printf("optimizer_reset=1\n");
    return 0;
}

static void usage(const char *program)
{
    fprintf(
        stderr,
        "usage: %s probe | init | status | prepare | ready | "
        "reconcile | recover-boot | cancel-unapplied | confirm-stable | "
        "preserve-stock-low | publish | snapshot | restore-snapshot | "
        "discard-snapshot | reset-for-rerun | reset | clear-stock-only\n",
        program);
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "probe") == 0) {
        flip_optimizer_calibration_t calibration;

        if (make_live_calibration(&calibration) < 0)
            return EXIT_FAILURE;

        print_plan(&calibration);
        return EXIT_SUCCESS;
    }

#if FLIP_OPTIMIZER_EXPERIMENTAL_BUILD
    if (strcmp(argv[1], "init-experimental-first") == 0) {
        flip_optimizer_calibration_t calibration;

        if (make_first_experimental_calibration(&calibration) < 0 ||
            flip_optimizer_calibration_save(&calibration) < 0)
            return EXIT_FAILURE;

        print_plan(&calibration);
        printf("experimental=1\n");
        printf("first_frequency_khz=408000\n");
        printf("first_candidate_uv=800000\n");
        printf("state=%s\n", FLIP_OPTIMIZER_CALIBRATION_FILE);
        return EXIT_SUCCESS;
    }
#endif

    if (strcmp(argv[1], "init") == 0) {
        flip_optimizer_calibration_t calibration;

        if (make_live_calibration(&calibration) < 0 ||
            apply_failure_history(&calibration) < 0 ||
            flip_optimizer_calibration_save(
                &calibration) < 0)
            return EXIT_FAILURE;

        print_plan(&calibration);
        printf(
            "state=%s\n",
            FLIP_OPTIMIZER_CALIBRATION_FILE);
        return EXIT_SUCCESS;
    }

    if (strcmp(argv[1], "status") == 0)
        return print_status() == 0
            ? EXIT_SUCCESS
            : EXIT_FAILURE;

    if (strcmp(argv[1], "prepare") == 0) {
        flip_optimizer_calibration_t calibration;
        size_t index = 0;
        unsigned uv = 0;

        if (flip_optimizer_calibration_load(
                &calibration) < 0)
            return EXIT_FAILURE;

        int rc =
            flip_optimizer_calibration_session_prepare(
                &calibration,
                &index,
                &uv);

        if (rc < 0)
            return EXIT_FAILURE;

        if (rc == 0) {
            printf("calibration_complete=1\n");
            return EXIT_SUCCESS;
        }

        printf("prepared=1\n");
        printf("opp_index=%zu\n", index);
        printf(
            "frequency_khz=%u\n",
            calibration.opps[index].frequency_khz);
        printf("candidate_uv=%u\n", uv);
        printf(
            "trial_id=%s\n",
            calibration.pending_trial_id);
        printf("voltage_written=0\n");
        return EXIT_SUCCESS;
    }

    if (strcmp(argv[1], "ready") == 0) {
        flip_optimizer_calibration_t calibration;

        if (flip_optimizer_calibration_load(
                &calibration) < 0)
            return EXIT_FAILURE;

        int ready =
            flip_optimizer_calibration_session_ready_for_apply(
                &calibration);

        printf("ready_for_apply=%d\n", ready);
        return ready ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (strcmp(argv[1], "reconcile") == 0) {
        flip_optimizer_calibration_t calibration;

        if (flip_optimizer_calibration_load(
                &calibration) < 0)
            return EXIT_FAILURE;

        int rc =
            flip_optimizer_calibration_session_reconcile(
                &calibration);

        if (rc < 0)
            return EXIT_FAILURE;

        printf("reconcile=%d\n", rc);
        return EXIT_SUCCESS;
    }

    if (strcmp(argv[1], "recover-boot") == 0) {
        flip_optimizer_calibration_t calibration;
        int reconcile = -1;
        int released = 0;
        int had_pending = 0;
        size_t pending_index = 0;
        unsigned pending_uv = 0;

        if (flip_optimizer_calibration_load(
                &calibration) < 0)
            return EXIT_FAILURE;

        had_pending = calibration.pending;
        if (had_pending) {
            pending_index = calibration.pending_index;
            pending_uv = calibration.pending_uv;
        }

        if (flip_optimizer_calibration_session_recover_boot(
                &calibration,
                &reconcile,
                &released) < 0)
            return EXIT_FAILURE;

        if (reconcile == FLIP_OPTIMIZER_RECONCILE_ROLLED_BACK &&
            had_pending) {
            if (append_history_unstable(
                    &calibration,
                    pending_index,
                    pending_uv,
                    "boot-rollback") < 0)
                fprintf(
                    stderr,
                    "optimizer history: rollback write failed frequency_khz=%u voltage_uv=%u\n",
                    calibration.opps[pending_index].frequency_khz,
                    pending_uv);

            /*
             * A hard rollback is enough evidence for this run. Do not keep
             * bisecting downward and deliberately crash the same OPP again.
             * Finish from the last proven-stable point plus the normal
             * production guardband. A future rerun can refine above this
             * persisted known-bad boundary without repeating the crash.
             */
            if (flip_optimizer_calibration_finalize_after_hard_rollback(
                    &calibration,
                    pending_index) < 0 ||
                flip_optimizer_calibration_save(
                    &calibration) < 0)
                return EXIT_FAILURE;

            printf(
                "hard_rollback_finalized frequency_khz=%u "
                "highest_unstable_uv=%u lowest_stable_uv=%u raw_final_uv=%u\n",
                calibration.opps[pending_index].frequency_khz,
                calibration.opps[pending_index].highest_unstable_uv,
                calibration.opps[pending_index].lowest_stable_uv,
                calibration.opps[pending_index].raw_final_uv);
        }

        printf("reconcile=%d\n", reconcile);
        printf(
            "stock_only_released=%d\n",
            released);
        return EXIT_SUCCESS;
    }
    if (strcmp(argv[1], "cancel-unapplied") == 0) {
        flip_optimizer_calibration_t calibration;

        if (flip_optimizer_calibration_load(
                &calibration) < 0 ||
            flip_optimizer_calibration_session_cancel_unapplied(
                &calibration) < 0)
            return EXIT_FAILURE;

        printf("cancelled_unapplied=1\n");
        return EXIT_SUCCESS;
    }
    if (strcmp(argv[1], "confirm-stable") == 0) {
        flip_optimizer_calibration_t calibration;

        if (flip_optimizer_calibration_load(
                &calibration) < 0 ||
            flip_optimizer_calibration_session_confirm_stable(
                &calibration) < 0)
            return EXIT_FAILURE;

        printf("confirmed_stable=1\n");
        return EXIT_SUCCESS;
    }

    if (strcmp(argv[1], "preserve-stock-low") == 0) {
        flip_optimizer_calibration_t calibration;

        if (flip_optimizer_calibration_load(
                &calibration) < 0 ||
            flip_optimizer_calibration_preserve_stock_below(
                &calibration,
                FLIP_OPTIMIZER_CALIBRATION_RK3566_MIN_TUNABLE_KHZ) < 0 ||
            flip_optimizer_calibration_save(
                &calibration) < 0)
            return EXIT_FAILURE;

        printf(
            "stock_preserved_below_khz=%u\n",
            FLIP_OPTIMIZER_CALIBRATION_RK3566_MIN_TUNABLE_KHZ);
        printf(
            "calibration_complete=%d\n",
            flip_optimizer_calibration_is_complete(
                &calibration));
        return EXIT_SUCCESS;
    }

    if (strcmp(argv[1], "publish") == 0)
        return publish_completed_calibration() == 0
            ? EXIT_SUCCESS
            : EXIT_FAILURE;

    if (strcmp(argv[1], "snapshot") == 0)
        return snapshot_optimizer_state() == 0
            ? EXIT_SUCCESS
            : EXIT_FAILURE;

    if (strcmp(argv[1], "restore-snapshot") == 0)
        return restore_optimizer_snapshot() == 0
            ? EXIT_SUCCESS
            : EXIT_FAILURE;

    if (strcmp(argv[1], "discard-snapshot") == 0)
        return discard_optimizer_snapshot() == 0
            ? EXIT_SUCCESS
            : EXIT_FAILURE;

    if (strcmp(argv[1], "reset-for-rerun") == 0)
        return reset_calibration_state(0) == 0
            ? EXIT_SUCCESS
            : EXIT_FAILURE;

    if (strcmp(argv[1], "reset") == 0)
        return reset_calibration_state(1) == 0
            ? EXIT_SUCCESS
            : EXIT_FAILURE;

    if (strcmp(argv[1], "clear-stock-only") == 0) {
        flip_optimizer_calibration_t calibration;

        if (flip_optimizer_calibration_load(
                &calibration) < 0 ||
            flip_optimizer_calibration_session_clear_stock_only(
                &calibration) < 0)
            return EXIT_FAILURE;

        printf("stock_only_cleared=1\n");
        return EXIT_SUCCESS;
    }

    usage(argv[0]);
    return EXIT_FAILURE;
}
