#include "flip_cpu_policy.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int make_path(char *path, size_t path_size, const char *name)
{
    if (!path || !path_size || !name)
        return -1;

    int n = snprintf(path, path_size, "%s/%s", FLIP_CPU_POLICY_ROOT, name);
    return n >= 0 && (size_t)n < path_size ? 0 : -1;
}

static int read_line(const char *name, char *buffer, size_t buffer_size)
{
    char path[512];

    if (make_path(path, sizeof(path), name) < 0 || !buffer || !buffer_size)
        return -1;

    FILE *f = fopen(path, "r");
    if (!f)
        return -1;

    if (!fgets(buffer, (int)buffer_size, f)) {
        fclose(f);
        return -1;
    }

    fclose(f);
    char *end = strpbrk(buffer, "\r\n");
    if (end)
        *end = '\0';

    return buffer[0] ? 0 : -1;
}

static int read_unsigned(const char *name, unsigned *value)
{
    char buffer[64];
    if (!value || read_line(name, buffer, sizeof(buffer)) < 0)
        return -1;

    errno = 0;
    char *end = NULL;
    unsigned long parsed = strtoul(buffer, &end, 10);

    if (errno || end == buffer || *end != '\0' || parsed > 0xffffffffUL)
        return -1;

    *value = (unsigned)parsed;
    return 0;
}

static int write_unsigned(const char *name, unsigned value)
{
    char path[512];
    if (make_path(path, sizeof(path), name) < 0)
        return -1;

    FILE *f = fopen(path, "w");
    if (!f)
        return -1;

    int ok = fprintf(f, "%u\n", value);
    if (fflush(f) != 0)
        ok = -1;
    if (fclose(f) != 0)
        ok = -1;

    return ok < 0 ? -1 : 0;
}

static int write_text(const char *name, const char *value)
{
    char path[512];
    if (!value || !value[0] || make_path(path, sizeof(path), name) < 0)
        return -1;

    FILE *f = fopen(path, "w");
    if (!f)
        return -1;

    int ok = fprintf(f, "%s\n", value);
    if (fflush(f) != 0)
        ok = -1;
    if (fclose(f) != 0)
        ok = -1;

    return ok < 0 ? -1 : 0;
}

int flip_cpu_policy_query_info(flip_cpu_policy_info_t *info)
{
    if (!info)
        return -1;

    memset(info, 0, sizeof(*info));

    char buffer[512];
    if (read_line("scaling_available_frequencies", buffer, sizeof(buffer)) < 0)
        return -1;

    char *save = NULL;
    char *token = strtok_r(buffer, " \t", &save);

    while (token) {
        if (info->frequency_count >= FLIP_CPU_POLICY_MAX_FREQS)
            return -1;

        errno = 0;
        char *end = NULL;
        unsigned long parsed = strtoul(token, &end, 10);

        if (errno || end == token || *end != '\0' || parsed > 0xffffffffUL)
            return -1;

        info->frequencies_khz[info->frequency_count++] = (unsigned)parsed;
        token = strtok_r(NULL, " \t", &save);
    }

    return info->frequency_count ? 0 : -1;
}

int flip_cpu_policy_snapshot(flip_cpu_policy_snapshot_t *snapshot)
{
    if (!snapshot)
        return -1;

    memset(snapshot, 0, sizeof(*snapshot));

    if (read_unsigned("scaling_min_freq", &snapshot->min_khz) < 0 ||
        read_unsigned("scaling_max_freq", &snapshot->max_khz) < 0 ||
        read_line("scaling_governor", snapshot->governor,
                  sizeof(snapshot->governor)) < 0)
        return -1;

    return 0;
}

static int frequency_supported(unsigned khz)
{
    flip_cpu_policy_info_t info;
    if (flip_cpu_policy_query_info(&info) < 0)
        return 0;

    for (size_t i = 0; i < info.frequency_count; ++i) {
        if (info.frequencies_khz[i] == khz)
            return 1;
    }

    return 0;
}

int flip_cpu_policy_set_limits(unsigned min_khz, unsigned max_khz)
{
    if (!min_khz || !max_khz || min_khz > max_khz ||
        !frequency_supported(min_khz) || !frequency_supported(max_khz))
        return -1;

    flip_cpu_policy_snapshot_t original;
    if (flip_cpu_policy_snapshot(&original) < 0)
        return -1;

    if (max_khz < original.min_khz) {
        if (write_unsigned("scaling_min_freq", min_khz) < 0)
            return -1;
        if (write_unsigned("scaling_max_freq", max_khz) < 0) {
            write_unsigned("scaling_min_freq", original.min_khz);
            return -1;
        }
    } else {
        if (write_unsigned("scaling_max_freq", max_khz) < 0)
            return -1;
        if (write_unsigned("scaling_min_freq", min_khz) < 0) {
            write_unsigned("scaling_max_freq", original.max_khz);
            return -1;
        }
    }

    return 0;
}

int flip_cpu_policy_set_max(unsigned max_khz)
{
    flip_cpu_policy_snapshot_t current;
    if (flip_cpu_policy_snapshot(&current) < 0)
        return -1;

    unsigned min_khz = current.min_khz;
    if (min_khz > max_khz)
        min_khz = max_khz;

    return flip_cpu_policy_set_limits(min_khz, max_khz);
}

int flip_cpu_policy_set_governor(const char *governor)
{
    if (!governor || !governor[0])
        return -1;

    return write_text("scaling_governor", governor);
}

int flip_cpu_policy_set_speed(unsigned khz)
{
    if (!frequency_supported(khz))
        return -1;

    char governor[FLIP_CPU_POLICY_GOVERNOR_MAX];

    if (read_line(
            "scaling_governor",
            governor,
            sizeof(governor)) < 0 ||
        strcmp(governor, "userspace") != 0)
        return -1;

    return write_unsigned("scaling_setspeed", khz);
}

int flip_cpu_policy_get_current(unsigned *khz)
{
    return read_unsigned("scaling_cur_freq", khz);
}

int flip_cpu_policy_restore(const flip_cpu_policy_snapshot_t *snapshot)
{
    if (!snapshot || !snapshot->governor[0])
        return -1;

    if (flip_cpu_policy_set_limits(snapshot->min_khz, snapshot->max_khz) < 0)
        return -1;

    return write_text("scaling_governor", snapshot->governor);
}
