#include "flip_optimizer.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int mkdir_if_needed(const char *path)
{
    if (mkdir(path, 0755) == 0 || errno == EEXIST)
        return 0;
    return -1;
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

static int parse_double(const char *text, double *value)
{
    if (!text || !text[0] || !value)
        return -1;

    errno = 0;
    char *end = NULL;
    double parsed = strtod(text, &end);

    if (errno || end == text || *end != '\0' || !isfinite(parsed))
        return -1;

    *value = parsed;
    return 0;
}

static int set_key(
    flip_optimizer_result_t *result,
    const char *key,
    const char *value)
{
    if (strcmp(key, "version") == 0) {
        unsigned v = 0;
        if (parse_unsigned(value, &v) < 0)
            return -1;
        result->version = (int)v;
        return 0;
    }

    if (strcmp(key, "device_id") == 0) {
        if (!value[0] || strlen(value) >= sizeof(result->device_id))
            return -1;
        strcpy(result->device_id, value);
        return 0;
    }

    if (strcmp(key, "model") == 0) {
        if (!value[0] || strlen(value) >= sizeof(result->model))
            return -1;
        strcpy(result->model, value);
        return 0;
    }

    if (strcmp(key, "calibrated_epoch") == 0)
        return parse_unsigned(value, &result->calibrated_epoch);

    if (strcmp(key, "guardband_uv") == 0)
        return parse_unsigned(value, &result->guardband_uv);

    if (strcmp(key, "baseline_runtime_minutes") == 0)
        return parse_unsigned(value, &result->baseline_runtime_minutes);

    if (strcmp(key, "stock_power_mw") == 0) {
        if (parse_double(value, &result->stock_power_mw) < 0)
            return -1;
        result->power_measurement_valid = 1;
        return 0;
    }

    if (strcmp(key, "optimized_power_mw") == 0) {
        if (parse_double(value, &result->optimized_power_mw) < 0)
            return -1;
        result->power_measurement_valid = 1;
        return 0;
    }

    if (strncmp(key, "opp", 3) == 0) {
        unsigned index = 0;
        char suffix[32];

        if (sscanf(key, "opp%u_%31s", &index, suffix) != 2 ||
            index >= FLIP_OPTIMIZER_MAX_OPPS)
            return -1;

        flip_optimizer_opp_t *opp = &result->opps[index];
        unsigned parsed = 0;

        if (parse_unsigned(value, &parsed) < 0)
            return -1;

        if (strcmp(suffix, "khz") == 0)
            opp->frequency_khz = parsed;
        else if (strcmp(suffix, "stock_uv") == 0)
            opp->stock_uv = parsed;
        else if (strcmp(suffix, "optimized_uv") == 0)
            opp->optimized_uv = parsed;
        else
            return -1;

        if ((size_t)index + 1 > result->opp_count)
            result->opp_count = (size_t)index + 1;

        return 0;
    }

    return -1;
}

int flip_optimizer_validate(const flip_optimizer_result_t *result)
{
    if (!result ||
        result->version != FLIP_OPTIMIZER_VERSION ||
        !result->device_id[0] ||
        !result->model[0] ||
        result->opp_count == 0 ||
        result->opp_count > FLIP_OPTIMIZER_MAX_OPPS)
        return -1;

    unsigned previous_khz = 0;
    unsigned previous_uv = 0;

    for (size_t i = 0; i < result->opp_count; ++i) {
        const flip_optimizer_opp_t *opp = &result->opps[i];

        if (!opp->frequency_khz ||
            !opp->stock_uv ||
            !opp->optimized_uv ||
            opp->optimized_uv > opp->stock_uv ||
            (i && opp->frequency_khz <= previous_khz) ||
            (i && opp->optimized_uv < previous_uv))
            return -1;

        previous_khz = opp->frequency_khz;
        previous_uv = opp->optimized_uv;
    }

    if (result->power_measurement_valid &&
        (result->stock_power_mw <= 0.0 ||
         result->optimized_power_mw <= 0.0 ||
         result->optimized_power_mw > result->stock_power_mw))
        return -1;

    return 0;
}

int flip_optimizer_load(flip_optimizer_result_t *result)
{
    if (!result)
        return -1;

    memset(result, 0, sizeof(*result));

    FILE *f = fopen(FLIP_OPTIMIZER_RESULT_FILE, "r");
    if (!f)
        return -1;

    char line[512];

    while (fgets(line, sizeof(line), f)) {
        char *end = strpbrk(line, "\r\n");
        if (end)
            *end = '\0';

        if (!line[0] || line[0] == '#')
            continue;

        char *eq = strchr(line, '=');
        if (!eq) {
            fclose(f);
            return -1;
        }

        *eq++ = '\0';

        if (set_key(result, line, eq) < 0) {
            fclose(f);
            return -1;
        }
    }

    if (fclose(f) != 0)
        return -1;

    return flip_optimizer_validate(result);
}

int flip_optimizer_save(const flip_optimizer_result_t *result)
{
    if (flip_optimizer_validate(result) < 0)
        return -1;

    if (mkdir_if_needed(FLIP_OPTIMIZER_ROOT) < 0)
        return -1;

    char tmp[512];
    int n = snprintf(tmp, sizeof(tmp), "%s.tmp", FLIP_OPTIMIZER_RESULT_FILE);

    if (n < 0 || (size_t)n >= sizeof(tmp))
        return -1;

    FILE *f = fopen(tmp, "w");
    if (!f)
        return -1;

    int failed = 0;

    failed |= fprintf(f, "version=%d\n", result->version) < 0;
    failed |= fprintf(f, "device_id=%s\n", result->device_id) < 0;
    failed |= fprintf(f, "model=%s\n", result->model) < 0;
    failed |= fprintf(f, "calibrated_epoch=%u\n",
                      result->calibrated_epoch) < 0;
    failed |= fprintf(f, "guardband_uv=%u\n", result->guardband_uv) < 0;
    failed |= fprintf(f, "baseline_runtime_minutes=%u\n",
                      result->baseline_runtime_minutes) < 0;

    if (result->power_measurement_valid) {
        failed |= fprintf(f, "stock_power_mw=%.3f\n",
                          result->stock_power_mw) < 0;
        failed |= fprintf(f, "optimized_power_mw=%.3f\n",
                          result->optimized_power_mw) < 0;
    }

    for (size_t i = 0; i < result->opp_count; ++i) {
        const flip_optimizer_opp_t *opp = &result->opps[i];

        failed |= fprintf(f, "opp%zu_khz=%u\n",
                          i, opp->frequency_khz) < 0;
        failed |= fprintf(f, "opp%zu_stock_uv=%u\n",
                          i, opp->stock_uv) < 0;
        failed |= fprintf(f, "opp%zu_optimized_uv=%u\n",
                          i, opp->optimized_uv) < 0;
    }

    if (fflush(f) != 0 || fsync(fileno(f)) != 0 || fclose(f) != 0)
        failed = 1;

    if (failed) {
        unlink(tmp);
        return -1;
    }

    if (rename(tmp, FLIP_OPTIMIZER_RESULT_FILE) != 0) {
        unlink(tmp);
        return -1;
    }

    return 0;
}

int flip_optimizer_summarize(
    const flip_optimizer_result_t *result,
    flip_optimizer_summary_t *summary)
{
    if (!summary || flip_optimizer_validate(result) < 0)
        return -1;

    memset(summary, 0, sizeof(*summary));

    const flip_optimizer_opp_t *top =
        &result->opps[result->opp_count - 1];

    summary->top_opp_voltage_reduction_mv =
        ((double)top->stock_uv - (double)top->optimized_uv) / 1000.0;

    double voltage_ratio =
        (double)top->optimized_uv / (double)top->stock_uv;

    summary->top_opp_cpu_dynamic_reduction_percent =
        (1.0 - voltage_ratio * voltage_ratio) * 100.0;

    if (result->power_measurement_valid) {
        summary->measured_power_valid = 1;
        summary->whole_device_power_reduction_percent =
            (1.0 -
             result->optimized_power_mw / result->stock_power_mw) *
            100.0;

        if (result->baseline_runtime_minutes > 0) {
            summary->estimated_runtime_minutes =
                (double)result->baseline_runtime_minutes *
                result->stock_power_mw /
                result->optimized_power_mw;

            summary->estimated_extra_runtime_minutes =
                summary->estimated_runtime_minutes -
                (double)result->baseline_runtime_minutes;
        }
    }

    return 0;
}
