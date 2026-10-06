#include "flip_optimizer_calibration.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int valid_text(
    const char *text,
    size_t capacity)
{
    return text &&
        text[0] &&
        strlen(text) < capacity &&
        !strchr(text, '\n') &&
        !strchr(text, '\r') &&
        !strchr(text, '=');
}

static int mkdir_if_needed(const char *path)
{
    if (mkdir(path, 0755) == 0 || errno == EEXIST)
        return 0;

    return -1;
}

static int fsync_root(void)
{
    int fd = open(FLIP_OPTIMIZER_CALIBRATION_ROOT, O_RDONLY);
    if (fd < 0)
        return -1;

    int rc = fsync(fd);
    int saved_errno = errno;
    close(fd);
    errno = saved_errno;
    return rc;
}

static int parse_unsigned(
    const char *text,
    unsigned *value)
{
    if (!text || !text[0] || !value)
        return -1;

    errno = 0;
    char *end = NULL;
    unsigned long parsed = strtoul(text, &end, 10);

    if (errno || end == text || *end != '\0' ||
        parsed > 0xffffffffUL)
        return -1;

    *value = (unsigned)parsed;
    return 0;
}

static unsigned min_unsigned(
    unsigned a,
    unsigned b)
{
    return a < b ? a : b;
}

static unsigned max_unsigned(
    unsigned a,
    unsigned b)
{
    return a > b ? a : b;
}

static unsigned guarded_final(
    const flip_optimizer_calibration_t *calibration,
    const flip_optimizer_calibration_opp_t *opp,
    unsigned reference_uv)
{
    unsigned long long candidate =
        (unsigned long long)reference_uv +
        calibration->guardband_uv;

    if (candidate > opp->stock_uv)
        return opp->stock_uv;

    return (unsigned)candidate;
}

int flip_optimizer_calibration_init(
    flip_optimizer_calibration_t *calibration,
    const char *device_id,
    const char *model,
    const flip_optimizer_calibration_stock_opp_t *opps,
    size_t opp_count,
    unsigned voltage_step_uv,
    unsigned guardband_uv)
{
    if (!calibration ||
        !valid_text(device_id, FLIP_OPTIMIZER_DEVICE_ID_MAX) ||
        !valid_text(model, FLIP_OPTIMIZER_MODEL_MAX) ||
        !opps ||
        opp_count == 0 ||
        opp_count > FLIP_OPTIMIZER_CALIBRATION_MAX_OPPS ||
        !voltage_step_uv ||
        guardband_uv < voltage_step_uv ||
        guardband_uv % voltage_step_uv != 0)
        return -1;

    memset(calibration, 0, sizeof(*calibration));

    calibration->version =
        FLIP_OPTIMIZER_CALIBRATION_VERSION;
    strcpy(calibration->device_id, device_id);
    strcpy(calibration->model, model);
    calibration->voltage_step_uv = voltage_step_uv;
    calibration->guardband_uv = guardband_uv;
    calibration->opp_count = opp_count;

    for (size_t i = 0; i < opp_count; ++i) {
        const flip_optimizer_calibration_stock_opp_t *source =
            &opps[i];
        flip_optimizer_calibration_opp_t *target =
            &calibration->opps[i];

        if (!source->frequency_khz ||
            !source->stock_uv ||
            !source->floor_uv ||
            source->floor_uv > source->stock_uv ||
            (source->stock_uv - source->floor_uv) %
                    voltage_step_uv != 0 ||
            (i && source->frequency_khz <=
                    opps[i - 1].frequency_khz) ||
            (i && source->stock_uv <
                    opps[i - 1].stock_uv))
            return -1;

        target->frequency_khz = source->frequency_khz;
        target->stock_uv = source->stock_uv;
        target->floor_uv = source->floor_uv;
        target->lowest_stable_uv = source->stock_uv;

        if (source->stock_uv == source->floor_uv) {
            target->raw_final_uv = source->stock_uv;
            target->complete = 1;
        }
    }

    return flip_optimizer_calibration_validate(calibration);
}

int flip_optimizer_calibration_validate(
    const flip_optimizer_calibration_t *calibration)
{
    if (!calibration ||
        calibration->version !=
            FLIP_OPTIMIZER_CALIBRATION_VERSION ||
        !valid_text(
            calibration->device_id,
            sizeof(calibration->device_id)) ||
        !valid_text(
            calibration->model,
            sizeof(calibration->model)) ||
        !calibration->voltage_step_uv ||
        calibration->guardband_uv <
            calibration->voltage_step_uv ||
        calibration->guardband_uv %
                calibration->voltage_step_uv != 0 ||
        calibration->opp_count == 0 ||
        calibration->opp_count >
            FLIP_OPTIMIZER_CALIBRATION_MAX_OPPS ||
        (calibration->pending != 0 &&
         calibration->pending != 1))
        return -1;

    unsigned previous_khz = 0;
    unsigned previous_stock_uv = 0;

    for (size_t i = 0;
         i < calibration->opp_count;
         ++i) {
        const flip_optimizer_calibration_opp_t *opp =
            &calibration->opps[i];

        if (!opp->frequency_khz ||
            !opp->stock_uv ||
            !opp->floor_uv ||
            opp->floor_uv > opp->stock_uv ||
            (opp->stock_uv - opp->floor_uv) %
                    calibration->voltage_step_uv != 0 ||
            opp->lowest_stable_uv < opp->floor_uv ||
            opp->lowest_stable_uv > opp->stock_uv ||
            (opp->stock_uv - opp->lowest_stable_uv) %
                    calibration->voltage_step_uv != 0 ||
            (opp->highest_unstable_uv &&
             (opp->highest_unstable_uv < opp->floor_uv ||
              opp->highest_unstable_uv >=
                    opp->lowest_stable_uv ||
              (opp->stock_uv -
                    opp->highest_unstable_uv) %
                    calibration->voltage_step_uv != 0)) ||
            (opp->raw_final_uv &&
             (opp->raw_final_uv < opp->floor_uv ||
              opp->raw_final_uv > opp->stock_uv ||
              (opp->stock_uv - opp->raw_final_uv) %
                    calibration->voltage_step_uv != 0)) ||
            (opp->complete != 0 && opp->complete != 1) ||
            (!!opp->complete != !!opp->raw_final_uv) ||
            (i && opp->frequency_khz <= previous_khz) ||
            (i && opp->stock_uv < previous_stock_uv))
            return -1;

        previous_khz = opp->frequency_khz;
        previous_stock_uv = opp->stock_uv;
    }

    if (calibration->pending) {
        if (calibration->pending_index >=
                calibration->opp_count ||
            !calibration->pending_uv ||
            !valid_text(
                calibration->pending_trial_id,
                sizeof(calibration->pending_trial_id)))
            return -1;

        const flip_optimizer_calibration_opp_t *opp =
            &calibration->opps[calibration->pending_index];

        if (opp->complete ||
            calibration->pending_uv < opp->floor_uv ||
            calibration->pending_uv >=
                opp->lowest_stable_uv ||
            opp->lowest_stable_uv -
                calibration->pending_uv !=
                calibration->voltage_step_uv)
            return -1;
    } else if (calibration->pending_index ||
               calibration->pending_uv ||
               calibration->pending_trial_id[0]) {
        return -1;
    }

    return 0;
}

int flip_optimizer_calibration_save(
    const flip_optimizer_calibration_t *calibration)
{
    if (flip_optimizer_calibration_validate(calibration) < 0)
        return -1;

    if (mkdir_if_needed(FLIP_OPTIMIZER_CALIBRATION_ROOT) < 0)
        return -1;

    char tmp[512];
    int n = snprintf(
        tmp,
        sizeof(tmp),
        "%s.tmp",
        FLIP_OPTIMIZER_CALIBRATION_FILE);

    if (n < 0 || (size_t)n >= sizeof(tmp))
        return -1;

    FILE *f = fopen(tmp, "w");
    if (!f)
        return -1;

    int failed = 0;

    failed |= fprintf(
        f,
        "version=%d\n",
        calibration->version) < 0;
    failed |= fprintf(
        f,
        "device_id=%s\n",
        calibration->device_id) < 0;
    failed |= fprintf(
        f,
        "model=%s\n",
        calibration->model) < 0;
    failed |= fprintf(
        f,
        "voltage_step_uv=%u\n",
        calibration->voltage_step_uv) < 0;
    failed |= fprintf(
        f,
        "guardband_uv=%u\n",
        calibration->guardband_uv) < 0;
    failed |= fprintf(
        f,
        "opp_count=%zu\n",
        calibration->opp_count) < 0;
    failed |= fprintf(
        f,
        "pending=%d\n",
        calibration->pending) < 0;
    failed |= fprintf(
        f,
        "pending_index=%zu\n",
        calibration->pending_index) < 0;
    failed |= fprintf(
        f,
        "pending_uv=%u\n",
        calibration->pending_uv) < 0;
    failed |= fprintf(
        f,
        "pending_trial_id=%s\n",
        calibration->pending_trial_id) < 0;

    for (size_t i = 0;
         i < calibration->opp_count;
         ++i) {
        const flip_optimizer_calibration_opp_t *opp =
            &calibration->opps[i];

        failed |= fprintf(
            f,
            "opp%zu_khz=%u\n",
            i,
            opp->frequency_khz) < 0;
        failed |= fprintf(
            f,
            "opp%zu_stock_uv=%u\n",
            i,
            opp->stock_uv) < 0;
        failed |= fprintf(
            f,
            "opp%zu_floor_uv=%u\n",
            i,
            opp->floor_uv) < 0;
        failed |= fprintf(
            f,
            "opp%zu_lowest_stable_uv=%u\n",
            i,
            opp->lowest_stable_uv) < 0;
        failed |= fprintf(
            f,
            "opp%zu_highest_unstable_uv=%u\n",
            i,
            opp->highest_unstable_uv) < 0;
        failed |= fprintf(
            f,
            "opp%zu_raw_final_uv=%u\n",
            i,
            opp->raw_final_uv) < 0;
        failed |= fprintf(
            f,
            "opp%zu_tests_run=%u\n",
            i,
            opp->tests_run) < 0;
        failed |= fprintf(
            f,
            "opp%zu_complete=%d\n",
            i,
            opp->complete) < 0;
    }

    if (fflush(f) != 0)
        failed = 1;
    if (!failed && fsync(fileno(f)) != 0)
        failed = 1;
    if (fclose(f) != 0)
        failed = 1;

    if (failed) {
        unlink(tmp);
        return -1;
    }

    if (rename(
            tmp,
            FLIP_OPTIMIZER_CALIBRATION_FILE) != 0) {
        unlink(tmp);
        return -1;
    }

    return fsync_root();
}

static int set_key(
    flip_optimizer_calibration_t *calibration,
    const char *key,
    const char *value)
{
    unsigned parsed = 0;

    if (strcmp(key, "version") == 0) {
        if (parse_unsigned(value, &parsed) < 0)
            return -1;
        calibration->version = (int)parsed;
        return 0;
    }

    if (strcmp(key, "device_id") == 0) {
        if (!valid_text(
                value,
                sizeof(calibration->device_id)))
            return -1;
        strcpy(calibration->device_id, value);
        return 0;
    }

    if (strcmp(key, "model") == 0) {
        if (!valid_text(
                value,
                sizeof(calibration->model)))
            return -1;
        strcpy(calibration->model, value);
        return 0;
    }

    if (strcmp(key, "pending_trial_id") == 0) {
        if (strlen(value) >=
                sizeof(calibration->pending_trial_id))
            return -1;
        strcpy(calibration->pending_trial_id, value);
        return 0;
    }

    if (parse_unsigned(value, &parsed) < 0)
        return -1;

    if (strcmp(key, "voltage_step_uv") == 0)
        calibration->voltage_step_uv = parsed;
    else if (strcmp(key, "guardband_uv") == 0)
        calibration->guardband_uv = parsed;
    else if (strcmp(key, "opp_count") == 0)
        calibration->opp_count = parsed;
    else if (strcmp(key, "pending") == 0)
        calibration->pending = (int)parsed;
    else if (strcmp(key, "pending_index") == 0)
        calibration->pending_index = parsed;
    else if (strcmp(key, "pending_uv") == 0)
        calibration->pending_uv = parsed;
    else if (strncmp(key, "opp", 3) == 0) {
        unsigned index = 0;
        char suffix[64];

        if (sscanf(
                key,
                "opp%u_%63s",
                &index,
                suffix) != 2 ||
            index >=
                FLIP_OPTIMIZER_CALIBRATION_MAX_OPPS)
            return -1;

        flip_optimizer_calibration_opp_t *opp =
            &calibration->opps[index];

        if (strcmp(suffix, "khz") == 0)
            opp->frequency_khz = parsed;
        else if (strcmp(suffix, "stock_uv") == 0)
            opp->stock_uv = parsed;
        else if (strcmp(suffix, "floor_uv") == 0)
            opp->floor_uv = parsed;
        else if (strcmp(
                     suffix,
                     "lowest_stable_uv") == 0)
            opp->lowest_stable_uv = parsed;
        else if (strcmp(
                     suffix,
                     "highest_unstable_uv") == 0)
            opp->highest_unstable_uv = parsed;
        else if (strcmp(
                     suffix,
                     "raw_final_uv") == 0)
            opp->raw_final_uv = parsed;
        else if (strcmp(
                     suffix,
                     "tests_run") == 0)
            opp->tests_run = parsed;
        else if (strcmp(
                     suffix,
                     "complete") == 0)
            opp->complete = (int)parsed;
        else
            return -1;
    } else {
        return -1;
    }

    return 0;
}

int flip_optimizer_calibration_load(
    flip_optimizer_calibration_t *calibration)
{
    if (!calibration)
        return -1;

    memset(calibration, 0, sizeof(*calibration));

    FILE *f = fopen(
        FLIP_OPTIMIZER_CALIBRATION_FILE,
        "r");
    if (!f)
        return -1;

    char line[512];

    while (fgets(line, sizeof(line), f)) {
        char *end = strpbrk(line, "\r\n");
        if (end)
            *end = '\0';

        char *eq = strchr(line, '=');
        if (!eq) {
            fclose(f);
            return -1;
        }

        *eq++ = '\0';

        if (set_key(calibration, line, eq) < 0) {
            fclose(f);
            return -1;
        }
    }

    if (fclose(f) != 0)
        return -1;

    return flip_optimizer_calibration_validate(calibration);
}

int flip_optimizer_calibration_prepare_next(
    flip_optimizer_calibration_t *calibration,
    size_t *opp_index,
    unsigned *candidate_uv)
{
    if (!calibration ||
        flip_optimizer_calibration_validate(calibration) < 0 ||
        calibration->pending)
        return -1;

    for (size_t reverse = calibration->opp_count;
         reverse > 0;
         --reverse) {
        size_t index = reverse - 1;
        flip_optimizer_calibration_opp_t *opp =
            &calibration->opps[index];

        if (opp->complete)
            continue;

        if (opp->lowest_stable_uv <
                opp->floor_uv +
                calibration->voltage_step_uv)
            return -1;

        unsigned candidate =
            opp->lowest_stable_uv -
            calibration->voltage_step_uv;

        calibration->pending = 1;
        calibration->pending_index = index;
        calibration->pending_uv = candidate;

        int n = snprintf(
            calibration->pending_trial_id,
            sizeof(calibration->pending_trial_id),
            "cpu-%u-%u-%u",
            opp->frequency_khz,
            candidate,
            opp->tests_run + 1);

        if (n < 0 ||
            (size_t)n >=
                sizeof(calibration->pending_trial_id)) {
            calibration->pending = 0;
            calibration->pending_index = 0;
            calibration->pending_uv = 0;
            calibration->pending_trial_id[0] = '\0';
            return -1;
        }

        if (opp_index)
            *opp_index = index;
        if (candidate_uv)
            *candidate_uv = candidate;

        return 1;
    }

    return 0;
}

static int finish_pending(
    flip_optimizer_calibration_t *calibration,
    int stable)
{
    size_t index = calibration->pending_index;
    unsigned candidate = calibration->pending_uv;
    flip_optimizer_calibration_opp_t *opp =
        &calibration->opps[index];

    opp->tests_run++;

    if (stable) {
        opp->lowest_stable_uv = candidate;

        if (candidate == opp->floor_uv) {
            opp->raw_final_uv = guarded_final(
                calibration,
                opp,
                candidate);
            opp->complete = 1;
        }
    } else {
        opp->highest_unstable_uv = candidate;
        opp->raw_final_uv = max_unsigned(
            opp->lowest_stable_uv,
            guarded_final(
                calibration,
                opp,
                candidate));
        opp->complete = 1;
    }

    calibration->pending = 0;
    calibration->pending_index = 0;
    calibration->pending_uv = 0;
    calibration->pending_trial_id[0] = '\0';

    return flip_optimizer_calibration_validate(calibration);
}

int flip_optimizer_calibration_record_pending(
    flip_optimizer_calibration_t *calibration,
    int stable)
{
    if (!calibration ||
        flip_optimizer_calibration_validate(calibration) < 0 ||
        !calibration->pending)
        return -1;

    return finish_pending(
        calibration,
        stable ? 1 : 0);
}

int flip_optimizer_calibration_record_rollback(
    flip_optimizer_calibration_t *calibration,
    const char *trial_id)
{
    if (!calibration ||
        !trial_id ||
        flip_optimizer_calibration_validate(calibration) < 0 ||
        !calibration->pending ||
        strcmp(
            calibration->pending_trial_id,
            trial_id) != 0)
        return -1;

    return finish_pending(calibration, 0);
}

int flip_optimizer_calibration_is_complete(
    const flip_optimizer_calibration_t *calibration)
{
    if (flip_optimizer_calibration_validate(calibration) < 0 ||
        calibration->pending)
        return 0;

    for (size_t i = 0;
         i < calibration->opp_count;
         ++i) {
        if (!calibration->opps[i].complete)
            return 0;
    }

    return 1;
}

int flip_optimizer_calibration_build_result(
    const flip_optimizer_calibration_t *calibration,
    unsigned calibrated_epoch,
    flip_optimizer_result_t *result)
{
    if (!result ||
        !flip_optimizer_calibration_is_complete(calibration))
        return -1;

    memset(result, 0, sizeof(*result));

    result->version = FLIP_OPTIMIZER_VERSION;
    strcpy(result->device_id, calibration->device_id);
    strcpy(result->model, calibration->model);
    result->calibrated_epoch = calibrated_epoch;
    result->guardband_uv = calibration->guardband_uv;
    result->opp_count = calibration->opp_count;

    unsigned previous_uv = 0;

    for (size_t i = 0;
         i < calibration->opp_count;
         ++i) {
        const flip_optimizer_calibration_opp_t *source =
            &calibration->opps[i];
        flip_optimizer_opp_t *target =
            &result->opps[i];

        target->frequency_khz = source->frequency_khz;
        target->stock_uv = source->stock_uv;
        target->optimized_uv = min_unsigned(
            source->stock_uv,
            max_unsigned(
                source->raw_final_uv,
                previous_uv));

        previous_uv = target->optimized_uv;
    }

    return flip_optimizer_validate(result);
}
