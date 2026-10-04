#define _GNU_SOURCE

#include "flip_telemetry.h"

#include "flip_battery.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

static int file_read_line(
    const char *path,
    char *buffer,
    size_t buffer_size)
{
    if (!path ||
        !path[0] ||
        !buffer ||
        buffer_size == 0)
        return -1;

    FILE *f = fopen(path, "r");

    if (!f)
        return -1;

    if (!fgets(buffer, (int)buffer_size, f)) {
        fclose(f);
        return -1;
    }

    fclose(f);

    char *newline = strchr(buffer, '\n');
    if (newline)
        *newline = '\0';

    return 0;
}

static int file_read_ull(
    const char *path,
    unsigned long long *value)
{
    char buffer[128];

    if (!value ||
        file_read_line(path, buffer, sizeof(buffer)) < 0)
        return -1;

    errno = 0;
    char *end = NULL;
    unsigned long long parsed =
        strtoull(buffer, &end, 10);

    if (errno != 0 ||
        end == buffer)
        return -1;

    *value = parsed;
    return 0;
}

static int file_read_long(
    const char *path,
    long *value)
{
    char buffer[128];

    if (!value ||
        file_read_line(path, buffer, sizeof(buffer)) < 0)
        return -1;

    errno = 0;
    char *end = NULL;
    long parsed =
        strtol(buffer, &end, 10);

    if (errno != 0 ||
        end == buffer)
        return -1;

    *value = parsed;
    return 0;
}

static int path_exists(
    const char *path)
{
    return path &&
           path[0] &&
           access(path, R_OK) == 0;
}

static int text_contains_ci(
    const char *text,
    const char *needle)
{
    if (!text || !needle)
        return 0;

    size_t needle_length = strlen(needle);

    if (needle_length == 0)
        return 1;

    for (const char *p = text;
         *p;
         ++p) {

        size_t i = 0;

        while (p[i] &&
               i < needle_length &&
               tolower((unsigned char)p[i]) ==
               tolower((unsigned char)needle[i])) {

            ++i;
        }

        if (i == needle_length)
            return 1;
    }

    return 0;
}

static int read_proc_cpu(
    uint64_t *total,
    uint64_t *idle)
{
    if (!total || !idle)
        return -1;

    FILE *f = fopen("/proc/stat", "r");

    if (!f)
        return -1;

    char line[512];

    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return -1;
    }

    fclose(f);

    unsigned long long user = 0;
    unsigned long long nice = 0;
    unsigned long long system = 0;
    unsigned long long idle_value = 0;
    unsigned long long iowait = 0;
    unsigned long long irq = 0;
    unsigned long long softirq = 0;
    unsigned long long steal = 0;

    int fields = sscanf(
        line,
        "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
        &user,
        &nice,
        &system,
        &idle_value,
        &iowait,
        &irq,
        &softirq,
        &steal);

    if (fields < 4)
        return -1;

    *idle =
        (uint64_t)idle_value +
        (fields >= 5
            ? (uint64_t)iowait
            : 0);

    *total =
        (uint64_t)user +
        (uint64_t)nice +
        (uint64_t)system +
        (uint64_t)idle_value +
        (fields >= 5
            ? (uint64_t)iowait
            : 0) +
        (fields >= 6
            ? (uint64_t)irq
            : 0) +
        (fields >= 7
            ? (uint64_t)softirq
            : 0) +
        (fields >= 8
            ? (uint64_t)steal
            : 0);

    return 0;
}

int flip_telemetry_cpu_percent(
    uint64_t previous_total,
    uint64_t previous_idle,
    uint64_t current_total,
    uint64_t current_idle,
    double *percent)
{
    if (!percent ||
        current_total <= previous_total ||
        current_idle < previous_idle)
        return -1;

    uint64_t total_delta =
        current_total - previous_total;

    uint64_t idle_delta =
        current_idle - previous_idle;

    if (idle_delta > total_delta)
        return -1;

    *percent =
        100.0 *
        (double)(total_delta - idle_delta) /
        (double)total_delta;

    if (*percent < 0.0)
        *percent = 0.0;

    if (*percent > 100.0)
        *percent = 100.0;

    return 0;
}

static void discover_cpu_freq_paths(
    flip_telemetry_t *telemetry)
{
    telemetry->cpu_freq_path_count = 0;

    for (int cpu = 0;
         cpu < 4;
         ++cpu) {

        char path[256];

        snprintf(
            path,
            sizeof(path),
            "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq",
            cpu);

        if (!path_exists(path)) {
            snprintf(
                path,
                sizeof(path),
                "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_cur_freq",
                cpu);
        }

        if (!path_exists(path))
            continue;

        snprintf(
            telemetry->cpu_freq_paths[
                telemetry->cpu_freq_path_count],
            sizeof(telemetry->cpu_freq_paths[0]),
            "%s",
            path);

        telemetry->cpu_freq_path_count++;
    }
}

static void discover_gpu_paths(
    flip_telemetry_t *telemetry)
{
    telemetry->gpu_freq_path[0] = '\0';
    telemetry->gpu_load_path[0] = '\0';

    DIR *dir = opendir("/sys/class/devfreq");

    if (dir) {
        struct dirent *entry;

        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] == '.')
                continue;

            char base[512];
            char name_path[512];
            char name_value[128];

            snprintf(
                base,
                sizeof(base),
                "/sys/class/devfreq/%s",
                entry->d_name);

            snprintf(
                name_path,
                sizeof(name_path),
                "%s/name",
                base);

            name_value[0] = '\0';
            file_read_line(
                name_path,
                name_value,
                sizeof(name_value));

            if (!text_contains_ci(entry->d_name, "gpu") &&
                !text_contains_ci(name_value, "gpu")) {

                continue;
            }

            char candidate[512];

            snprintf(
                candidate,
                sizeof(candidate),
                "%s/cur_freq",
                base);

            if (path_exists(candidate)) {
                snprintf(
                    telemetry->gpu_freq_path,
                    sizeof(telemetry->gpu_freq_path),
                    "%s",
                    candidate);
            }

            const char *load_names[] = {
                "load",
                "utilization",
                "utilisation",
                "busy",
            };

            for (size_t i = 0;
                 i < sizeof(load_names) /
                     sizeof(load_names[0]);
                 ++i) {

                snprintf(
                    candidate,
                    sizeof(candidate),
                    "%s/%s",
                    base,
                    load_names[i]);

                if (path_exists(candidate)) {
                    snprintf(
                        telemetry->gpu_load_path,
                        sizeof(telemetry->gpu_load_path),
                        "%s",
                        candidate);
                    break;
                }
            }

            if (telemetry->gpu_freq_path[0] ||
                telemetry->gpu_load_path[0])
                break;
        }

        closedir(dir);
    }

    if (!telemetry->gpu_load_path[0]) {
        const char *candidates[] = {
            "/sys/devices/platform/fde60000.gpu/utilisation",
            "/sys/devices/platform/fde60000.gpu/utilization",
            "/sys/devices/platform/fde60000.gpu/load",
        };

        for (size_t i = 0;
             i < sizeof(candidates) /
                 sizeof(candidates[0]);
             ++i) {

            if (path_exists(candidates[i])) {
                snprintf(
                    telemetry->gpu_load_path,
                    sizeof(telemetry->gpu_load_path),
                    "%s",
                    candidates[i]);
                break;
            }
        }
    }
}

static void discover_temperature_path(
    flip_telemetry_t *telemetry)
{
    telemetry->temp_path[0] = '\0';

    char fallback[256] = "";

    for (int zone = 0;
         zone < 16;
         ++zone) {

        char type_path[256];
        char temp_path[256];
        char type[128];

        snprintf(
            type_path,
            sizeof(type_path),
            "/sys/class/thermal/thermal_zone%d/type",
            zone);

        snprintf(
            temp_path,
            sizeof(temp_path),
            "/sys/class/thermal/thermal_zone%d/temp",
            zone);

        if (!path_exists(temp_path))
            continue;

        if (!fallback[0]) {
            snprintf(
                fallback,
                sizeof(fallback),
                "%s",
                temp_path);
        }

        type[0] = '\0';
        file_read_line(type_path, type, sizeof(type));

        if (text_contains_ci(type, "cpu") ||
            text_contains_ci(type, "soc") ||
            text_contains_ci(type, "package")) {

            snprintf(
                telemetry->temp_path,
                sizeof(telemetry->temp_path),
                "%s",
                temp_path);
            break;
        }
    }

    if (!telemetry->temp_path[0] &&
        fallback[0]) {

        snprintf(
            telemetry->temp_path,
            sizeof(telemetry->temp_path),
            "%s",
            fallback);
    }
}

int flip_telemetry_init(
    flip_telemetry_t *telemetry)
{
    if (!telemetry)
        return -1;

    memset(
        telemetry,
        0,
        sizeof(*telemetry));

    uint64_t total = 0;
    uint64_t idle = 0;

    if (read_proc_cpu(
            &total,
            &idle) == 0) {

        telemetry->previous_cpu_total = total;
        telemetry->previous_cpu_idle = idle;
        telemetry->cpu_baseline_valid = 1;
    }

    discover_cpu_freq_paths(telemetry);
    discover_gpu_paths(telemetry);
    discover_temperature_path(telemetry);

    fprintf(
        stderr,
        "FLIP_TELEMETRY: cpu_freq_sources=%d gpu_freq=%s gpu_load=%s temp=%s battery=%s\n",
        telemetry->cpu_freq_path_count,
        telemetry->gpu_freq_path[0]
            ? telemetry->gpu_freq_path
            : "unavailable",
        telemetry->gpu_load_path[0]
            ? telemetry->gpu_load_path
            : "unavailable",
        telemetry->temp_path[0]
            ? telemetry->temp_path
            : "unavailable",
        FLIP_BATTERY_ROOT);

    return 0;
}

static void sample_cpu(
    flip_telemetry_t *telemetry)
{
    uint64_t total = 0;
    uint64_t idle = 0;

    telemetry->snapshot.cpu_percent_valid = 0;

    if (read_proc_cpu(
            &total,
            &idle) == 0) {

        if (telemetry->cpu_baseline_valid &&
            flip_telemetry_cpu_percent(
                telemetry->previous_cpu_total,
                telemetry->previous_cpu_idle,
                total,
                idle,
                &telemetry->snapshot.cpu_percent) == 0) {

            telemetry->snapshot.cpu_percent_valid = 1;
        }

        telemetry->previous_cpu_total = total;
        telemetry->previous_cpu_idle = idle;
        telemetry->cpu_baseline_valid = 1;
    }

    telemetry->snapshot.cpu_khz_valid = 0;
    telemetry->snapshot.cpu_khz = 0;

    for (int i = 0;
         i < telemetry->cpu_freq_path_count;
         ++i) {

        unsigned long long value = 0;

        if (file_read_ull(
                telemetry->cpu_freq_paths[i],
                &value) < 0) {

            continue;
        }

        if (!telemetry->snapshot.cpu_khz_valid ||
            value > telemetry->snapshot.cpu_khz) {

            telemetry->snapshot.cpu_khz = value;
            telemetry->snapshot.cpu_khz_valid = 1;
        }
    }
}

static void sample_gpu(
    flip_telemetry_t *telemetry)
{
    telemetry->snapshot.gpu_percent_valid = 0;
    telemetry->snapshot.gpu_hz_valid = 0;

    if (telemetry->gpu_freq_path[0]) {
        unsigned long long value = 0;

        if (file_read_ull(
                telemetry->gpu_freq_path,
                &value) == 0) {

            if (value > 0 &&
                value < 10000000ULL) {

                value *= 1000ULL;
            }

            telemetry->snapshot.gpu_hz = value;
            telemetry->snapshot.gpu_hz_valid = 1;
        }
    }

    if (telemetry->gpu_load_path[0]) {
        char buffer[128];

        if (file_read_line(
                telemetry->gpu_load_path,
                buffer,
                sizeof(buffer)) == 0) {

            errno = 0;
            char *end = NULL;
            double value =
                strtod(buffer, &end);

            if (errno == 0 &&
                end != buffer &&
                value >= 0.0 &&
                value <= 100.0) {

                telemetry->snapshot.gpu_percent = value;
                telemetry->snapshot.gpu_percent_valid = 1;
            }
        }
    }
}

static void sample_temperature(
    flip_telemetry_t *telemetry)
{
    telemetry->snapshot.temp_valid = 0;

    if (!telemetry->temp_path[0])
        return;

    long value = 0;

    if (file_read_long(
            telemetry->temp_path,
            &value) < 0) {

        return;
    }

    if (value > -200 &&
        value < 300) {

        value *= 1000;
    }

    if (value < -200000 ||
        value > 300000)
        return;

    telemetry->snapshot.temp_millic = value;
    telemetry->snapshot.temp_valid = 1;
}

static void sample_battery(
    flip_telemetry_t *telemetry)
{
    telemetry->snapshot.battery_valid = 0;

    flip_battery_snapshot_t battery;

    if (flip_battery_read(
            &battery) < 0 ||
        !battery.percent_valid) {
        return;
    }

    telemetry->snapshot.battery_percent =
        battery.percent;

    telemetry->snapshot.battery_valid = 1;
}

int flip_telemetry_sample(
    flip_telemetry_t *telemetry,
    double fps,
    int fps_valid)
{
    if (!telemetry)
        return -1;

    telemetry->snapshot.fps = fps;
    telemetry->snapshot.fps_valid =
        fps_valid ? 1 : 0;

    sample_cpu(telemetry);
    sample_gpu(telemetry);
    sample_temperature(telemetry);
    sample_battery(telemetry);

    return 0;
}

const flip_telemetry_snapshot_t *
flip_telemetry_snapshot(
    const flip_telemetry_t *telemetry)
{
    if (!telemetry)
        return NULL;

    return &telemetry->snapshot;
}

void flip_telemetry_format_overlay(
    const flip_telemetry_snapshot_t *snapshot,
    char lines[FLIP_TELEMETRY_OVERLAY_LINES]
              [FLIP_TELEMETRY_OVERLAY_LINE_LENGTH])
{
    if (!lines)
        return;

    for (int i = 0;
         i < FLIP_TELEMETRY_OVERLAY_LINES;
         ++i) {

        lines[i][0] = '\0';
    }

    if (!snapshot) {
        snprintf(lines[0], FLIP_TELEMETRY_OVERLAY_LINE_LENGTH, "FPS --");
        snprintf(lines[1], FLIP_TELEMETRY_OVERLAY_LINE_LENGTH, "CPU --");
        snprintf(lines[2], FLIP_TELEMETRY_OVERLAY_LINE_LENGTH, "GPU --");
        snprintf(lines[3], FLIP_TELEMETRY_OVERLAY_LINE_LENGTH, "TMP --");
        snprintf(lines[4], FLIP_TELEMETRY_OVERLAY_LINE_LENGTH, "BAT --");
        return;
    }

    if (snapshot->fps_valid) {
        snprintf(
            lines[0],
            FLIP_TELEMETRY_OVERLAY_LINE_LENGTH,
            "FPS %.1f",
            snapshot->fps);
    } else {
        snprintf(lines[0], FLIP_TELEMETRY_OVERLAY_LINE_LENGTH, "FPS --");
    }

    if (snapshot->cpu_percent_valid &&
        snapshot->cpu_khz_valid) {

        snprintf(
            lines[1],
            FLIP_TELEMETRY_OVERLAY_LINE_LENGTH,
            "CPU %.0f%% %lluM",
            snapshot->cpu_percent,
            snapshot->cpu_khz / 1000ULL);

    } else if (snapshot->cpu_percent_valid) {

        snprintf(
            lines[1],
            FLIP_TELEMETRY_OVERLAY_LINE_LENGTH,
            "CPU %.0f%%",
            snapshot->cpu_percent);

    } else if (snapshot->cpu_khz_valid) {

        snprintf(
            lines[1],
            FLIP_TELEMETRY_OVERLAY_LINE_LENGTH,
            "CPU -- %lluM",
            snapshot->cpu_khz / 1000ULL);

    } else {
        snprintf(lines[1], FLIP_TELEMETRY_OVERLAY_LINE_LENGTH, "CPU --");
    }

    if (snapshot->gpu_percent_valid &&
        snapshot->gpu_hz_valid) {

        snprintf(
            lines[2],
            FLIP_TELEMETRY_OVERLAY_LINE_LENGTH,
            "GPU %.0f%% %lluM",
            snapshot->gpu_percent,
            snapshot->gpu_hz / 1000000ULL);

    } else if (snapshot->gpu_percent_valid) {

        snprintf(
            lines[2],
            FLIP_TELEMETRY_OVERLAY_LINE_LENGTH,
            "GPU %.0f%%",
            snapshot->gpu_percent);

    } else if (snapshot->gpu_hz_valid) {

        snprintf(
            lines[2],
            FLIP_TELEMETRY_OVERLAY_LINE_LENGTH,
            "GPU -- %lluM",
            snapshot->gpu_hz / 1000000ULL);

    } else {
        snprintf(lines[2], FLIP_TELEMETRY_OVERLAY_LINE_LENGTH, "GPU --");
    }

    if (snapshot->temp_valid) {
        snprintf(
            lines[3],
            FLIP_TELEMETRY_OVERLAY_LINE_LENGTH,
            "TMP %ldC",
            snapshot->temp_millic / 1000L);
    } else {
        snprintf(lines[3], FLIP_TELEMETRY_OVERLAY_LINE_LENGTH, "TMP --");
    }

    if (snapshot->battery_valid) {
        snprintf(
            lines[4],
            FLIP_TELEMETRY_OVERLAY_LINE_LENGTH,
            "BAT %d%%",
            snapshot->battery_percent);
    } else {
        snprintf(lines[4], FLIP_TELEMETRY_OVERLAY_LINE_LENGTH, "BAT --");
    }
}
