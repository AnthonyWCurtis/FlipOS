#define _GNU_SOURCE

#include "flip_battery.h"
#include "flip_paths.h"
#include "flip_telemetry.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define PERF_METRICS_DIR FLIP_STORAGE_ROOT "/metrics"
#define PERF_CPU_POLICY "/sys/devices/system/cpu/cpufreq/policy0"
#define PERF_GPU_ROOT "/sys/class/devfreq/fde60000.gpu"

static int read_line(
    const char *path,
    char *buffer,
    size_t buffer_size)
{
    if (!path || !buffer || buffer_size == 0)
        return -1;

    FILE *f = fopen(path, "r");
    if (!f)
        return -1;

    if (!fgets(buffer, (int)buffer_size, f)) {
        fclose(f);
        return -1;
    }

    fclose(f);

    char *newline = strpbrk(buffer, "\r\n");
    if (newline)
        *newline = '\0';

    return 0;
}

static int read_long(
    const char *path,
    long *value)
{
    char text[128];

    if (!value ||
        read_line(path, text, sizeof(text)) < 0)
        return -1;

    errno = 0;
    char *end = NULL;
    long parsed = strtol(text, &end, 10);

    if (errno != 0 ||
        end == text ||
        *end != '\0')
        return -1;

    *value = parsed;
    return 0;
}

static void sanitize_label(
    const char *source,
    char *target,
    size_t target_size)
{
    if (!target || target_size == 0)
        return;

    size_t out = 0;

    for (size_t i = 0;
         source && source[i] && out + 1 < target_size;
         ++i) {

        unsigned char c = (unsigned char)source[i];

        if (isalnum(c) ||
            c == '-' ||
            c == '_') {
            target[out++] = (char)c;
        } else {
            target[out++] = '_';
        }
    }

    if (out == 0 && target_size > 1) {
        memcpy(target, "capture", 7);
        out = 7;
    }

    target[out] = '\0';
}

static int parse_positive(
    const char *text,
    unsigned *value)
{
    if (!text || !value || !text[0])
        return -1;

    errno = 0;
    char *end = NULL;
    unsigned long parsed = strtoul(text, &end, 10);

    if (errno != 0 ||
        end == text ||
        *end != '\0' ||
        parsed == 0 ||
        parsed > UINT_MAX)
        return -1;

    *value = (unsigned)parsed;
    return 0;
}

static int key_value(
    const char *path,
    const char *key,
    char *value,
    size_t value_size)
{
    if (!path || !key || !value || value_size == 0)
        return -1;

    value[0] = '\0';

    FILE *f = fopen(path, "r");
    if (!f)
        return -1;

    char line[1024];
    size_t key_length = strlen(key);

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, key_length) != 0 ||
            line[key_length] != '=')
            continue;

        const char *source = line + key_length + 1;
        snprintf(value, value_size, "%s", source);

        char *newline = strpbrk(value, "\r\n");
        if (newline)
            *newline = '\0';

        fclose(f);
        return 0;
    }

    fclose(f);
    return -1;
}

static int session_value(
    const char *key,
    char *value,
    size_t value_size)
{
    return key_value(
        FLIP_SESSION_FILE,
        key,
        value,
        value_size);
}

static int process_name_matches(
    const char *cmdline,
    const char *name)
{
    if (!cmdline || !name)
        return 0;

    const char *base = strrchr(cmdline, '/');
    base = base ? base + 1 : cmdline;

    return strcmp(base, name) == 0;
}

static pid_t find_process(
    const char *name)
{
    DIR *dir = opendir("/proc");
    if (!dir)
        return -1;

    pid_t found = -1;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (!isdigit((unsigned char)entry->d_name[0]))
            continue;

        char *end = NULL;
        long pid_value = strtol(entry->d_name, &end, 10);

        if (!end ||
            *end != '\0' ||
            pid_value <= 0)
            continue;

        char path[PATH_MAX];

        snprintf(
            path,
            sizeof(path),
            "/proc/%ld/cmdline",
            pid_value);

        FILE *f = fopen(path, "r");
        if (!f)
            continue;

        char command[PATH_MAX];
        size_t count = fread(
            command,
            1,
            sizeof(command) - 1,
            f);

        fclose(f);

        if (count == 0)
            continue;

        command[count] = '\0';

        if (process_name_matches(command, name)) {
            found = (pid_t)pid_value;
            break;
        }
    }

    closedir(dir);
    return found;
}

static int discover_thermal(
    const char *wanted_type,
    char *path,
    size_t path_size)
{
    if (!wanted_type ||
        !path ||
        path_size == 0)
        return -1;

    path[0] = '\0';

    for (int index = 0;
         index < 16;
         ++index) {

        char type_path[PATH_MAX];
        char temp_path[PATH_MAX];
        char type[128];

        snprintf(
            type_path,
            sizeof(type_path),
            "/sys/class/thermal/thermal_zone%d/type",
            index);

        if (read_line(
                type_path,
                type,
                sizeof(type)) < 0)
            continue;

        if (strcmp(type, wanted_type) != 0)
            continue;

        snprintf(
            temp_path,
            sizeof(temp_path),
            "/sys/class/thermal/thermal_zone%d/temp",
            index);

        if (access(temp_path, R_OK) != 0)
            continue;

        snprintf(path, path_size, "%s", temp_path);
        return 0;
    }

    return -1;
}

static void format_pid(
    pid_t pid,
    char *buffer,
    size_t buffer_size)
{
    if (pid > 0)
        snprintf(buffer, buffer_size, "%ld", (long)pid);
    else if (buffer_size > 0)
        buffer[0] = '\0';
}

static void timespec_add_seconds(
    struct timespec *value,
    unsigned seconds)
{
    value->tv_sec += (time_t)seconds;
}

static int write_sample(
    FILE *out,
    const char *label,
    flip_telemetry_t *telemetry,
    const char *soc_temp_path,
    const char *gpu_temp_path)
{
    if (flip_telemetry_sample(
            telemetry,
            0.0,
            0) < 0)
        return -1;

    const flip_telemetry_snapshot_t *snapshot =
        flip_telemetry_snapshot(telemetry);

    if (!snapshot)
        return -1;

    char session_state[64];
    char session_system[64];
    char session_pid[64];

    session_value(
        "state",
        session_state,
        sizeof(session_state));

    session_value(
        "system",
        session_system,
        sizeof(session_system));

    session_value(
        "pid",
        session_pid,
        sizeof(session_pid));

    char volume_percent[32] = "";
    char brightness_saved[32] = "";

    key_value(
        FLIP_SETTINGS_FILE,
        "volume",
        volume_percent,
        sizeof(volume_percent));

    key_value(
        FLIP_SETTINGS_FILE,
        "brightness",
        brightness_saved,
        sizeof(brightness_saved));

    long brightness_actual = 0;
    int brightness_actual_valid =
        read_long(
            "/sys/class/backlight/backlight/actual_brightness",
            &brightness_actual) == 0;

    pid_t gb_pid = find_process("flip-gb");
    pid_t gba_pid = find_process("flip-gba");

    const char *live_mode = "FRONTEND";
    pid_t live_pid = -1;

    if (gba_pid > 0) {
        live_mode = "GBA";
        live_pid = gba_pid;
    } else if (gb_pid > 0) {
        live_mode =
            strcmp(session_system, "GBC") == 0
                ? "GBC"
                : "GB";
        live_pid = gb_pid;
    }

    char live_pid_text[32];
    char frontend_pid_text[32];
    char gpu_service_pid_text[32];
    char aplay_pid_text[32];

    format_pid(
        live_pid,
        live_pid_text,
        sizeof(live_pid_text));

    format_pid(
        find_process("flip-frontend"),
        frontend_pid_text,
        sizeof(frontend_pid_text));

    format_pid(
        find_process("flip-gpu-service"),
        gpu_service_pid_text,
        sizeof(gpu_service_pid_text));

    format_pid(
        find_process("aplay"),
        aplay_pid_text,
        sizeof(aplay_pid_text));

    char cpu_governor[64] = "";
    char gpu_governor[64] = "";

    read_line(
        PERF_CPU_POLICY "/scaling_governor",
        cpu_governor,
        sizeof(cpu_governor));

    read_line(
        PERF_GPU_ROOT "/governor",
        gpu_governor,
        sizeof(gpu_governor));

    long soc_temp = 0;
    long gpu_temp = 0;

    int soc_temp_valid =
        soc_temp_path &&
        soc_temp_path[0] &&
        read_long(soc_temp_path, &soc_temp) == 0;

    int gpu_temp_valid =
        gpu_temp_path &&
        gpu_temp_path[0] &&
        read_long(gpu_temp_path, &gpu_temp) == 0;

    flip_battery_snapshot_t battery;
    int battery_ok =
        flip_battery_read(&battery) == 0;

    long charge_counter = 0;
    int charge_counter_valid =
        read_long(
            FLIP_BATTERY_ROOT "/charge_counter",
            &charge_counter) == 0;

    long long power_mw = 0;
    int power_valid =
        battery_ok &&
        battery.voltage_valid &&
        battery.current_valid;

    if (power_valid) {
        power_mw =
            ((long long)battery.voltage_uv *
             (long long)battery.current_ua) /
            1000000000LL;
    }

    struct timespec monotonic;
    clock_gettime(
        CLOCK_MONOTONIC,
        &monotonic);

    time_t epoch = time(NULL);

#define PRINT_VALID_INT(valid, value) \
    do { if (valid) fprintf(out, "%lld", (long long)(value)); } while (0)

#define PRINT_VALID_DOUBLE(valid, value) \
    do { if (valid) fprintf(out, "%.2f", (double)(value)); } while (0)

    fprintf(
        out,
        "%lld,%lld,%s,%s,%s,%s,%s,%s,%s,%s,",
        (long long)epoch,
        (long long)monotonic.tv_sec,
        label,
        live_mode,
        live_pid_text,
        session_state,
        session_system,
        session_pid,
        volume_percent,
        brightness_saved);

    PRINT_VALID_INT(
        brightness_actual_valid,
        brightness_actual);
    fputc(',', out);

    PRINT_VALID_DOUBLE(
        snapshot->cpu_percent_valid,
        snapshot->cpu_percent);
    fputc(',', out);

    PRINT_VALID_INT(
        snapshot->cpu_khz_valid,
        snapshot->cpu_khz);
    fprintf(out, ",%s,", cpu_governor);

    PRINT_VALID_DOUBLE(
        snapshot->gpu_percent_valid,
        snapshot->gpu_percent);
    fputc(',', out);

    PRINT_VALID_INT(
        snapshot->gpu_hz_valid,
        snapshot->gpu_hz);
    fprintf(out, ",%s,", gpu_governor);

    PRINT_VALID_INT(
        soc_temp_valid,
        soc_temp);
    fputc(',', out);

    PRINT_VALID_INT(
        gpu_temp_valid,
        gpu_temp);
    fputc(',', out);

    PRINT_VALID_INT(
        battery_ok && battery.percent_valid,
        battery.percent);
    fputc(',', out);

    if (battery_ok && battery.status_valid) {
        fputs(
            flip_battery_status_name(
                battery.status),
            out);
    }
    fputc(',', out);

    PRINT_VALID_INT(
        battery_ok && battery.voltage_valid,
        battery.voltage_uv);
    fputc(',', out);

    PRINT_VALID_INT(
        battery_ok && battery.current_valid,
        battery.current_ua);
    fputc(',', out);

    PRINT_VALID_INT(
        power_valid,
        power_mw);
    fputc(',', out);

    PRINT_VALID_INT(
        charge_counter_valid,
        charge_counter);

    fprintf(
        out,
        ",%s,%s,%s\n",
        frontend_pid_text,
        gpu_service_pid_text,
        aplay_pid_text);

#undef PRINT_VALID_INT
#undef PRINT_VALID_DOUBLE

    fflush(out);
    return 0;
}

int main(
    int argc,
    char **argv)
{
    const char *label_arg =
        argc >= 2
            ? argv[1]
            : "baseline";

    unsigned duration = 60;
    unsigned interval = 1;

    if (argc >= 3 &&
        parse_positive(
            argv[2],
            &duration) < 0) {

        fprintf(
            stderr,
            "invalid duration: %s\n",
            argv[2]);
        return 2;
    }

    if (argc >= 4 &&
        parse_positive(
            argv[3],
            &interval) < 0) {

        fprintf(
            stderr,
            "invalid interval: %s\n",
            argv[3]);
        return 2;
    }

    char label[128];
    sanitize_label(
        label_arg,
        label,
        sizeof(label));

    if (mkdir(
            PERF_METRICS_DIR,
            0755) < 0 &&
        errno != EEXIST) {

        fprintf(
            stderr,
            "cannot create %s: %s\n",
            PERF_METRICS_DIR,
            strerror(errno));
        return 1;
    }

    char output_path[PATH_MAX];

    if (argc >= 5) {
        snprintf(
            output_path,
            sizeof(output_path),
            "%s",
            argv[4]);
    } else {
        time_t now = time(NULL);
        struct tm local_time;

        localtime_r(
            &now,
            &local_time);

        char stamp[32];

        strftime(
            stamp,
            sizeof(stamp),
            "%Y%m%d-%H%M%S",
            &local_time);

        snprintf(
            output_path,
            sizeof(output_path),
            PERF_METRICS_DIR "/%s-%s.csv",
            stamp,
            label);
    }

    FILE *out = fopen(
        output_path,
        "w");

    if (!out) {
        fprintf(
            stderr,
            "cannot open %s: %s\n",
            output_path,
            strerror(errno));
        return 1;
    }

    fprintf(
        out,
        "epoch,uptime_s,label,live_mode,live_runner_pid,"
        "session_state,session_system,session_pid,"
        "volume_percent,brightness_saved,brightness_actual,"
        "cpu_percent,cpu_khz,cpu_governor,"
        "gpu_percent,gpu_hz,gpu_governor,"
        "soc_temp_mc,gpu_temp_mc,"
        "battery_percent,battery_status,battery_uv,battery_ua,"
        "battery_power_mw,charge_counter,"
        "frontend_pid,gpu_service_pid,aplay_pid\n");

    flip_telemetry_t telemetry;

    if (flip_telemetry_init(
            &telemetry) < 0) {

        fclose(out);
        return 1;
    }

    char soc_temp_path[PATH_MAX];
    char gpu_temp_path[PATH_MAX];

    discover_thermal(
        "soc-thermal",
        soc_temp_path,
        sizeof(soc_temp_path));

    discover_thermal(
        "gpu-thermal",
        gpu_temp_path,
        sizeof(gpu_temp_path));

    struct timespec started;
    struct timespec next;

    clock_gettime(
        CLOCK_MONOTONIC,
        &started);

    next = started;

    for (;;) {
        timespec_add_seconds(
            &next,
            interval);

        int sleep_rc;

        do {
            sleep_rc =
                clock_nanosleep(
                    CLOCK_MONOTONIC,
                    TIMER_ABSTIME,
                    &next,
                    NULL);
        } while (sleep_rc == EINTR);

        if (sleep_rc != 0) {
            fclose(out);
            return 1;
        }

        struct timespec now;

        clock_gettime(
            CLOCK_MONOTONIC,
            &now);

        time_t elapsed =
            now.tv_sec - started.tv_sec;

        if (elapsed > (time_t)duration)
            break;

        if (write_sample(
                out,
                label,
                &telemetry,
                soc_temp_path,
                gpu_temp_path) < 0) {

            fclose(out);
            return 1;
        }

        if (elapsed >= (time_t)duration)
            break;
    }

    fclose(out);

    printf("%s\n", output_path);
    return 0;
}
