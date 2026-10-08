#include "flip_cpu_policy.h"
#include "flip_optimizer_calibration.h"
#include "flip_optimizer_calibration_session.h"
#include "flip_optimizer_guard.h"
#include "flip_optimizer_hw_watchdog.h"
#include "flip_optimizer_panic_policy.h"
#include "flip_optimizer_supervisor.h"
#include "flip_optimizer_quiesce.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/reboot.h>
#include <linux/input.h>
#include <pthread.h>
#include <sys/wait.h>
#include <signal.h>
#include <poll.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#ifndef FLIP_OPTIMIZER_OPP_ROOT
#define FLIP_OPTIMIZER_OPP_ROOT "/sys/kernel/debug/opp/cpu0"
#endif

#ifndef FLIP_OPTIMIZER_REGULATOR_ROOT
#define FLIP_OPTIMIZER_REGULATOR_ROOT "/sys/class/regulator"
#endif

#ifndef FLIP_OPTIMIZER_MODULE_PATH
#define FLIP_OPTIMIZER_MODULE_PATH "/mnt/SDCARD/flipos-dev/lib/flip_opp_voltage.ko"
#endif

#define FLIP_OPTIMIZER_MODULE_NAME "flip_opp_voltage"
#define FLIP_OPTIMIZER_TSADC_MODULE_PATH \
    "/mnt/SDCARD/flipos-dev/lib/flip_tsadc_recovery.ko"
#define FLIP_OPTIMIZER_TSADC_MODULE_NAME "flip_tsadc_recovery"
#define FLIP_OPTIMIZER_TSADC_CONTROL \
    "/sys/module/flip_tsadc_recovery/parameters/recovery"
#define FLIP_OPTIMIZER_DEADMAN_PATH \
    "/mnt/SDCARD/flipos-dev/bin/flip-optimizer-deadman"
#define FLIP_OPTIMIZER_DEADMAN_HEADROOM_SECONDS 20U
#define FLIP_OPTIMIZER_HW_WATCHDOG_REQUEST_SECONDS 8U
#define FLIP_OPTIMIZER_MODULE_CONTROL \
    "/sys/module/flip_opp_voltage/parameters/control"
#define FLIP_OPTIMIZER_UI_RETURN_FILE \
    FLIP_OPTIMIZER_CALIBRATION_ROOT "/ui-return"
#define FLIP_OPTIMIZER_AUTO_SESSION_FILE \
    FLIP_OPTIMIZER_CALIBRATION_ROOT "/auto-session"
#define FLIP_OPTIMIZER_AUTO_SESSION_TMP \
    FLIP_OPTIMIZER_CALIBRATION_ROOT "/auto-session.tmp"
#define FLIP_OPTIMIZER_USB_ONLINE \
    "/sys/class/power_supply/usb/online"
#define FLIP_OPTIMIZER_AC_ONLINE \
    "/sys/class/power_supply/ac/online"
#define FLIP_OPTIMIZER_RUNTIME_TOOL \
    "/mnt/SDCARD/flipos-dev/bin/flip-optimizer-runtime"
#define FLIP_OPTIMIZER_VALIDATION_REQUEST \
    FLIP_OPTIMIZER_CALIBRATION_ROOT "/validation-request"
#define FLIP_OPTIMIZER_VALIDATION_ACTIVE \
    FLIP_OPTIMIZER_CALIBRATION_ROOT "/validation-active"
#define FLIP_OPTIMIZER_VALIDATION_RESULT \
    FLIP_OPTIMIZER_CALIBRATION_ROOT "/validation-result"
#define FLIP_OPTIMIZER_VALIDATION_RETURN \
    FLIP_OPTIMIZER_CALIBRATION_ROOT "/validation-return"

#define FLIP_OPTIMIZER_SOC_THERMAL_TYPE \
    "/sys/class/thermal/thermal_zone0/type"
#define FLIP_OPTIMIZER_SOC_THERMAL_TEMP \
    "/sys/class/thermal/thermal_zone0/temp"
#define FLIP_OPTIMIZER_VALIDATION_PARK_KHZ 408000U
#define FLIP_OPTIMIZER_STRESS_WORKERS 4U
#define FLIP_OPTIMIZER_STRESS_MIN_SECONDS 5U
#define FLIP_OPTIMIZER_STRESS_MAX_SECONDS 120U
#define FLIP_OPTIMIZER_STRESS_MAX_TEMP_MC 75000U
#define FLIP_OPTIMIZER_STRESS_BUFFER_WORDS 32768U
#define FLIP_OPTIMIZER_INTEGRITY_WORDS 4096U
#define FLIP_OPTIMIZER_INTEGRITY_ROUNDS 48U

typedef struct {
    unsigned target_uv;
    unsigned min_uv;
    unsigned max_uv;
} flip_opp_voltage_triplet_t;

static int read_text_path(
    const char *path,
    char *buffer,
    size_t size)
{
    if (!path || !buffer || size < 2)
        return -1;

    FILE *f = fopen(path, "r");
    if (!f)
        return -1;

    if (!fgets(buffer, (int)size, f)) {
        fclose(f);
        return -1;
    }

    if (fclose(f) != 0)
        return -1;

    char *end = strpbrk(buffer, "\r\n");
    if (end)
        *end = '\0';

    return buffer[0] ? 0 : -1;
}

static int parse_unsigned_text(
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

static int read_unsigned_path(
    const char *path,
    unsigned *value)
{
    char text[64];

    if (read_text_path(path, text, sizeof(text)) < 0)
        return -1;

    return parse_unsigned_text(text, value);
}

static int write_text_path(
    const char *path,
    const char *value)
{
    if (!path || !value || !value[0])
        return -1;

    FILE *f = fopen(path, "w");
    if (!f)
        return -1;

    int failed = fprintf(f, "%s\n", value) < 0;

    if (!failed && fflush(f) != 0)
        failed = 1;
    if (fclose(f) != 0)
        failed = 1;

    return failed ? -1 : 0;
}

static int optimizer_external_power_online(void)
{
    unsigned usb = 0;
    unsigned ac = 0;
    int usb_ok =
        read_unsigned_path(
            FLIP_OPTIMIZER_USB_ONLINE,
            &usb) == 0;
    int ac_ok =
        read_unsigned_path(
            FLIP_OPTIMIZER_AC_ONLINE,
            &ac) == 0;

    if ((usb_ok && usb > 0) ||
        (ac_ok && ac > 0))
        return 1;

    if (usb_ok || ac_ok)
        return 0;

    return -1;
}

static int optimizer_ui_power_interlock_active(void)
{
    return access(
        FLIP_OPTIMIZER_UI_RETURN_FILE,
        F_OK) == 0;
}

static int clear_ui_optimizer_session_files(void)
{
    int failed = 0;

    if (unlink(
            FLIP_OPTIMIZER_AUTO_SESSION_FILE) < 0 &&
        errno != ENOENT)
        failed = 1;

    if (unlink(
            FLIP_OPTIMIZER_AUTO_SESSION_TMP) < 0 &&
        errno != ENOENT)
        failed = 1;

    return failed ? -1 : 0;
}

static int make_opp_supply_path(
    char *path,
    size_t size,
    unsigned freq_khz,
    const char *name)
{
    unsigned long long hz =
        (unsigned long long)freq_khz * 1000ULL;

    int n = snprintf(
        path,
        size,
        "%s/opp:%llu/supply-0/%s",
        FLIP_OPTIMIZER_OPP_ROOT,
        hz,
        name);

    return n >= 0 && (size_t)n < size ? 0 : -1;
}

static int read_opp_triplet(
    unsigned freq_khz,
    flip_opp_voltage_triplet_t *triplet)
{
    if (!triplet)
        return -1;

    memset(triplet, 0, sizeof(*triplet));

    char path[512];

    if (make_opp_supply_path(
            path,
            sizeof(path),
            freq_khz,
            "u_volt_target") < 0 ||
        read_unsigned_path(
            path,
            &triplet->target_uv) < 0 ||
        make_opp_supply_path(
            path,
            sizeof(path),
            freq_khz,
            "u_volt_min") < 0 ||
        read_unsigned_path(
            path,
            &triplet->min_uv) < 0 ||
        make_opp_supply_path(
            path,
            sizeof(path),
            freq_khz,
            "u_volt_max") < 0 ||
        read_unsigned_path(
            path,
            &triplet->max_uv) < 0)
        return -1;

    return 0;
}

static int triplet_matches(
    const flip_opp_voltage_triplet_t *left,
    const flip_opp_voltage_triplet_t *right)
{
    return left &&
        right &&
        left->target_uv == right->target_uv &&
        left->min_uv == right->min_uv &&
        left->max_uv == right->max_uv;
}

static int discover_cpu_regulator(
    char *directory,
    size_t size)
{
    DIR *root = opendir(FLIP_OPTIMIZER_REGULATOR_ROOT);
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
            FLIP_OPTIMIZER_REGULATOR_ROOT,
            entry->d_name);

        if (n < 0 || (size_t)n >= sizeof(path) ||
            read_text_path(
                path,
                name,
                sizeof(name)) < 0 ||
            strcmp(name, "vdd_cpu") != 0)
            continue;

        n = snprintf(
            directory,
            size,
            "%s/%s",
            FLIP_OPTIMIZER_REGULATOR_ROOT,
            entry->d_name);

        if (n >= 0 && (size_t)n < size)
            rc = 0;

        break;
    }

    closedir(root);
    return rc;
}

static int read_cpu_rail_uv(unsigned *uv)
{
    char directory[512];
    char path[640];

    if (!uv ||
        discover_cpu_regulator(
            directory,
            sizeof(directory)) < 0)
        return -1;

    int n = snprintf(
        path,
        sizeof(path),
        "%s/microvolts",
        directory);

    if (n < 0 || (size_t)n >= sizeof(path))
        return -1;

    return read_unsigned_path(path, uv);
}

static int choose_parking_frequency(
    const flip_cpu_policy_info_t *info,
    unsigned target_khz,
    const flip_cpu_policy_snapshot_t *original,
    unsigned *parking_khz)
{
    if (!info || !original || !parking_khz)
        return -1;

    size_t target_index = info->frequency_count;

    for (size_t i = 0; i < info->frequency_count; ++i) {
        if (info->frequencies_khz[i] == target_khz) {
            target_index = i;
            break;
        }
    }

    if (target_index == info->frequency_count)
        return -1;

    unsigned candidate = 0;

    if (target_index > 0)
        candidate = info->frequencies_khz[target_index - 1];
    else if (target_index + 1 < info->frequency_count)
        candidate = info->frequencies_khz[target_index + 1];
    else
        return -1;

    if (candidate < original->min_khz ||
        candidate > original->max_khz ||
        target_khz > original->max_khz)
        return -1;

    *parking_khz = candidate;
    return 0;
}

static int wait_for_frequency(
    unsigned expected_khz)
{
    const struct timespec delay = {
        .tv_sec = 0,
        .tv_nsec = 20 * 1000 * 1000,
    };

    for (unsigned attempt = 0; attempt < 100; ++attempt) {
        unsigned current = 0;

        if (flip_cpu_policy_get_current(&current) == 0 &&
            current == expected_khz)
            return 0;

        nanosleep(&delay, NULL);
    }

    return -1;
}

static int park_cpu(
    const flip_cpu_policy_snapshot_t *original,
    unsigned target_khz,
    unsigned parking_khz)
{
    if (!original)
        return -1;

    if (parking_khz < target_khz) {
        if (flip_cpu_policy_set_max(parking_khz) < 0)
            return -1;
    }

    if (flip_cpu_policy_set_governor("userspace") < 0)
        return -1;

    if (flip_cpu_policy_set_speed(parking_khz) < 0 ||
        wait_for_frequency(parking_khz) < 0)
        return -1;

    return 0;
}

typedef struct {
    unsigned worker_index;
    atomic_int *stop;
    atomic_int *failed;
    uint64_t checksum;
} flip_stress_worker_t;

typedef struct {
    unsigned worker_index;
    uint64_t checksum;
    int rc;
} flip_integrity_worker_t;

static uint64_t rotate_left_64(
    uint64_t value,
    unsigned shift)
{
    return
        (value << shift) |
        (value >> (64U - shift));
}

static void *integrity_worker_main(void *opaque)
{
    static const uint64_t expected[
        FLIP_OPTIMIZER_STRESS_WORKERS] = {
        0x5265b85ac09d6289ULL,
        0x1ac7c67083926e93ULL,
        0x8bf9170181793689ULL,
        0xd32cf2f5bb3f25bbULL,
    };

    flip_integrity_worker_t *worker = opaque;

    if (!worker ||
        worker->worker_index >=
            FLIP_OPTIMIZER_STRESS_WORKERS)
        return NULL;

    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(worker->worker_index, &set);

    if (pthread_setaffinity_np(
            pthread_self(),
            sizeof(set),
            &set) != 0) {
        worker->rc = -1;
        return NULL;
    }

    uint64_t *buffer = malloc(
        FLIP_OPTIMIZER_INTEGRITY_WORDS *
        sizeof(*buffer));

    if (!buffer) {
        worker->rc = -1;
        return NULL;
    }

    const uint64_t c1 =
        0x5851f42d4c957f2dULL;
    const uint64_t c2 =
        0x94d049bb133111ebULL;
    const uint64_t base =
        0xd1b54a32d192ed03ULL;
    const uint64_t worker_constant =
        0x9e3779b97f4a7c15ULL;

    uint64_t seed =
        base ^
        ((uint64_t)worker->worker_index *
         worker_constant);

    for (size_t i = 0;
         i < FLIP_OPTIMIZER_INTEGRITY_WORDS;
         ++i) {
        buffer[i] =
            seed ^
            ((uint64_t)i * c1);
    }

    uint64_t state = seed;

    for (unsigned round = 0;
         round < FLIP_OPTIMIZER_INTEGRITY_ROUNDS;
         ++round) {
        for (size_t i = 0;
             i < FLIP_OPTIMIZER_INTEGRITY_WORDS;
             ++i) {
            state ^= state << 13;
            state ^= state >> 7;
            state ^= state << 17;

            uint64_t value =
                buffer[i] +
                state +
                ((uint64_t)i * c1) +
                round;

            value = rotate_left_64(
                value,
                23);

            buffer[i] =
                value ^
                (state * c2);
        }

        state ^=
            buffer[
                (state >> 16) %
                FLIP_OPTIMIZER_INTEGRITY_WORDS];
    }

    uint64_t checksum = state;

    for (size_t i = 0;
         i < FLIP_OPTIMIZER_INTEGRITY_WORDS;
         i += 17) {
        checksum ^=
            rotate_left_64(
                buffer[i],
                29);
    }

    worker->checksum = checksum;
    worker->rc =
        checksum ==
            expected[worker->worker_index]
        ? 0
        : -1;

    free(buffer);
    return NULL;
}

static int run_integrity_check(void)
{
    pthread_t threads[FLIP_OPTIMIZER_STRESS_WORKERS];
    flip_integrity_worker_t
        workers[FLIP_OPTIMIZER_STRESS_WORKERS];

    memset(threads, 0, sizeof(threads));
    memset(workers, 0, sizeof(workers));

    unsigned created = 0;
    int failed = 0;

    for (unsigned i = 0;
         i < FLIP_OPTIMIZER_STRESS_WORKERS;
         ++i) {
        workers[i].worker_index = i;
        workers[i].rc = -1;

        if (pthread_create(
                &threads[i],
                NULL,
                integrity_worker_main,
                &workers[i]) != 0) {
            failed = 1;
            break;
        }

        created++;
    }

    for (unsigned i = 0; i < created; ++i) {
        if (pthread_join(
                threads[i],
                NULL) != 0)
            failed = 1;
    }

    if (created != FLIP_OPTIMIZER_STRESS_WORKERS)
        failed = 1;

    for (unsigned i = 0; i < created; ++i) {
        if (workers[i].rc != 0) {
            fprintf(
                stderr,
                "OPTIMIZER_CANDIDATE: integrity mismatch "
                "worker=%u checksum=%016llx\n",
                i,
                (unsigned long long)
                    workers[i].checksum);
            failed = 1;
        }
    }

    return failed ? -1 : 0;
}

static int read_soc_temp_mc(unsigned *temp_mc)
{
    char type[64];

    if (!temp_mc ||
        read_text_path(
            FLIP_OPTIMIZER_SOC_THERMAL_TYPE,
            type,
            sizeof(type)) < 0 ||
        strcmp(type, "soc-thermal") != 0)
        return -1;

    return read_unsigned_path(
        FLIP_OPTIMIZER_SOC_THERMAL_TEMP,
        temp_mc);
}

static void *stress_worker_main(void *opaque)
{
    flip_stress_worker_t *worker = opaque;

    if (!worker || !worker->stop || !worker->failed)
        return NULL;

    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(worker->worker_index, &set);

    if (pthread_setaffinity_np(
            pthread_self(),
            sizeof(set),
            &set) != 0) {
        atomic_store(worker->failed, 1);
        return NULL;
    }

    uint64_t *buffer = calloc(
        FLIP_OPTIMIZER_STRESS_BUFFER_WORDS,
        sizeof(*buffer));

    if (!buffer) {
        atomic_store(worker->failed, 1);
        return NULL;
    }

    uint64_t state =
        0x9e3779b97f4a7c15ULL ^
        ((uint64_t)worker->worker_index << 32);
    double fp =
        1.000001 +
        (double)worker->worker_index * 0.000001;

    while (!atomic_load_explicit(
               worker->stop,
               memory_order_relaxed)) {
        for (size_t i = 0;
             i < FLIP_OPTIMIZER_STRESS_BUFFER_WORDS;
             ++i) {
            state ^= state << 13;
            state ^= state >> 7;
            state ^= state << 17;

            uint64_t value =
                buffer[i] +
                state +
                (uint64_t)i * 0x5851f42d4c957f2dULL;

            value =
                (value << 23) |
                (value >> (64 - 23));

            buffer[i] =
                value ^
                (state * 0x94d049bb133111ebULL);
        }

        for (unsigned i = 0; i < 4096; ++i) {
            fp =
                fp * 1.00000011920928955078125 +
                0.00000095367431640625;
            if (fp > 2.0)
                fp -= 1.0;
        }

        state ^=
            buffer[
                (state >> 16) %
                FLIP_OPTIMIZER_STRESS_BUFFER_WORDS];
    }

    worker->checksum =
        state ^
        buffer[state % FLIP_OPTIMIZER_STRESS_BUFFER_WORDS] ^
        (uint64_t)(fp * 1000000000.0);

    free(buffer);
    return NULL;
}

static int run_stress_window(
    unsigned duration_seconds,
    unsigned target_khz,
    unsigned candidate_uv,
    flip_optimizer_hw_watchdog_t *hw_watchdog,
    int require_external_power,
    int *power_cancelled,
    unsigned *max_temp_mc,
    unsigned *sample_count)
{
    if (duration_seconds <
            FLIP_OPTIMIZER_STRESS_MIN_SECONDS ||
        duration_seconds >
            FLIP_OPTIMIZER_STRESS_MAX_SECONDS ||
        !target_khz ||
        !candidate_uv)
        return -1;

    if (power_cancelled)
        *power_cancelled = 0;

    pthread_t threads[FLIP_OPTIMIZER_STRESS_WORKERS];
    flip_stress_worker_t
        workers[FLIP_OPTIMIZER_STRESS_WORKERS];

    memset(threads, 0, sizeof(threads));
    memset(workers, 0, sizeof(workers));

    atomic_int stop = 0;
    atomic_int failed = 0;
    unsigned created = 0;

    for (unsigned i = 0;
         i < FLIP_OPTIMIZER_STRESS_WORKERS;
         ++i) {
        workers[i].worker_index = i;
        workers[i].stop = &stop;
        workers[i].failed = &failed;

        if (pthread_create(
                &threads[i],
                NULL,
                stress_worker_main,
                &workers[i]) != 0) {
            atomic_store(&failed, 1);
            break;
        }

        created++;
    }

    struct timespec start;
    if (clock_gettime(CLOCK_MONOTONIC, &start) != 0)
        atomic_store(&failed, 1);

    unsigned local_max_temp = 0;
    unsigned local_samples = 0;

    const struct timespec monitor_delay = {
        .tv_sec = 0,
        .tv_nsec = 100 * 1000 * 1000,
    };

    while (!atomic_load(&failed)) {
        struct timespec now;

        if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
            atomic_store(&failed, 1);
            break;
        }

        long long elapsed_ms =
            (long long)(now.tv_sec - start.tv_sec) * 1000LL +
            (long long)(now.tv_nsec - start.tv_nsec) / 1000000LL;

        if (elapsed_ms >=
            (long long)duration_seconds * 1000LL)
            break;

        unsigned current_khz = 0;
        unsigned rail_uv = 0;
        unsigned temp_mc = 0;

        if (require_external_power) {
            int power_online =
                optimizer_external_power_online();

            if (power_online <= 0) {
                if (power_cancelled)
                    *power_cancelled = 1;

                fprintf(
                    stderr,
                    "OPTIMIZER_CANDIDATE: CHARGER_REMOVED cancel=1\n");
                break;
            }
        }

        if (flip_optimizer_hw_watchdog_ping(
                hw_watchdog) < 0 ||
            flip_cpu_policy_get_current(&current_khz) < 0 ||
            current_khz != target_khz ||
            read_cpu_rail_uv(&rail_uv) < 0 ||
            rail_uv != candidate_uv ||
            read_soc_temp_mc(&temp_mc) < 0 ||
            temp_mc >= FLIP_OPTIMIZER_STRESS_MAX_TEMP_MC) {
            fprintf(
                stderr,
                "OPTIMIZER_CANDIDATE: live monitor failed "
                "freq=%u/%u rail_uv=%u/%u temp_mc=%u limit=%u\n",
                current_khz,
                target_khz,
                rail_uv,
                candidate_uv,
                temp_mc,
                FLIP_OPTIMIZER_STRESS_MAX_TEMP_MC);
            atomic_store(&failed, 1);
            break;
        }

        if (temp_mc > local_max_temp)
            local_max_temp = temp_mc;

        local_samples++;
        nanosleep(&monitor_delay, NULL);
    }

    atomic_store(&stop, 1);

    for (unsigned i = 0; i < created; ++i) {
        if (pthread_join(threads[i], NULL) != 0)
            atomic_store(&failed, 1);
    }

    if (created != FLIP_OPTIMIZER_STRESS_WORKERS)
        atomic_store(&failed, 1);

    uint64_t checksum = 0;
    for (unsigned i = 0; i < created; ++i)
        checksum ^= workers[i].checksum;

    if (!checksum)
        atomic_store(&failed, 1);

    if (max_temp_mc)
        *max_temp_mc = local_max_temp;
    if (sample_count)
        *sample_count = local_samples;

    return atomic_load(&failed) ? -1 : 0;
}

static int enter_candidate_frequency(
    const flip_cpu_policy_snapshot_t *original,
    unsigned target_khz,
    unsigned parking_khz)
{
    if (!original)
        return -1;

    if (target_khz > parking_khz) {
        if (flip_cpu_policy_set_limits(
                original->min_khz,
                target_khz) < 0)
            return -1;
    }

    if (flip_cpu_policy_set_speed(target_khz) < 0 ||
        wait_for_frequency(target_khz) < 0)
        return -1;

    return 0;
}

static int return_to_parking_frequency(
    unsigned parking_khz)
{
    if (flip_cpu_policy_set_speed(parking_khz) < 0 ||
        wait_for_frequency(parking_khz) < 0)
        return -1;

    return 0;
}

static int module_is_loaded(void)
{
    struct stat st;
    return stat("/sys/module/" FLIP_OPTIMIZER_MODULE_NAME, &st) == 0;
}

static int load_module(void)
{
    if (module_is_loaded())
        return -1;

    int fd = open(
        FLIP_OPTIMIZER_MODULE_PATH,
        O_RDONLY | O_CLOEXEC);

    if (fd < 0)
        return -1;

    long rc = syscall(SYS_finit_module, fd, "", 0);
    int saved_errno = errno;
    close(fd);
    errno = saved_errno;

    return rc == 0 ? 0 : -1;
}

static int unload_module(void)
{
    if (!module_is_loaded())
        return 0;

    long rc = syscall(
        SYS_delete_module,
        FLIP_OPTIMIZER_MODULE_NAME,
        O_NONBLOCK);

    return rc == 0 ? 0 : -1;
}

static int module_apply_table(
    unsigned freq_khz,
    unsigned candidate_uv,
    const flip_opp_voltage_triplet_t *stock)
{
    if (!stock)
        return -1;

    char command[256];

    int n = snprintf(
        command,
        sizeof(command),
        "apply,%u,%u,%u,%u,%u",
        freq_khz,
        candidate_uv,
        stock->target_uv,
        stock->min_uv,
        stock->max_uv);

    if (n < 0 || (size_t)n >= sizeof(command))
        return -1;

    return write_text_path(
        FLIP_OPTIMIZER_MODULE_CONTROL,
        command);
}

static int module_restore_table(void)
{
    return write_text_path(
        FLIP_OPTIMIZER_MODULE_CONTROL,
        "restore");
}

static int tsadc_recovery_is_loaded(void)
{
    struct stat st;
    return stat(
        "/sys/module/" FLIP_OPTIMIZER_TSADC_MODULE_NAME,
        &st) == 0;
}

static int tsadc_recovery_load(void)
{
    if (tsadc_recovery_is_loaded())
        return -1;

    int fd = open(
        FLIP_OPTIMIZER_TSADC_MODULE_PATH,
        O_RDONLY | O_CLOEXEC);

    if (fd < 0)
        return -1;

    long rc = syscall(SYS_finit_module, fd, "", 0);
    int saved_errno = errno;
    close(fd);
    errno = saved_errno;

    return rc == 0 ? 0 : -1;
}

static int tsadc_recovery_set(int enabled)
{
    return write_text_path(
        FLIP_OPTIMIZER_TSADC_CONTROL,
        enabled ? "1" : "0");
}

static int tsadc_recovery_unload(void)
{
    if (!tsadc_recovery_is_loaded())
        return 0;

    long rc = syscall(
        SYS_delete_module,
        FLIP_OPTIMIZER_TSADC_MODULE_NAME,
        O_NONBLOCK);

    return rc == 0 ? 0 : -1;
}

static int arm_deadman(
    unsigned timeout_seconds,
    pid_t *pid_out)
{
    if (!pid_out ||
        timeout_seconds < FLIP_OPTIMIZER_STRESS_MIN_SECONDS ||
        timeout_seconds >
            FLIP_OPTIMIZER_STRESS_MAX_SECONDS +
            FLIP_OPTIMIZER_DEADMAN_HEADROOM_SECONDS)
        return -1;

    int ready_pipe[2];
    if (pipe(ready_pipe) < 0)
        return -1;

    pid_t pid = fork();
    if (pid < 0) {
        close(ready_pipe[0]);
        close(ready_pipe[1]);
        return -1;
    }

    if (pid == 0) {
        close(ready_pipe[0]);

        char seconds_text[32];
        char fd_text[32];
        snprintf(
            seconds_text,
            sizeof(seconds_text),
            "%u",
            timeout_seconds);
        snprintf(
            fd_text,
            sizeof(fd_text),
            "%d",
            ready_pipe[1]);

        execl(
            FLIP_OPTIMIZER_DEADMAN_PATH,
            FLIP_OPTIMIZER_DEADMAN_PATH,
            "arm",
            seconds_text,
            fd_text,
            (char *)NULL);
        _exit(127);
    }

    close(ready_pipe[1]);

    struct pollfd pfd = {
        .fd = ready_pipe[0],
        .events = POLLIN,
    };

    int poll_rc = poll(&pfd, 1, 3000);
    char ready = 0;
    ssize_t read_rc =
        poll_rc > 0
            ? read(ready_pipe[0], &ready, 1)
            : -1;
    close(ready_pipe[0]);

    if (poll_rc <= 0 ||
        read_rc != 1 ||
        ready != 'R') {
        kill(pid, SIGKILL);
        (void)waitpid(pid, NULL, 0);
        return -1;
    }

    *pid_out = pid;
    return 0;
}

static int disarm_deadman(pid_t *pid)
{
    if (!pid || *pid <= 0)
        return 0;

    pid_t target = *pid;
    *pid = -1;

    if (kill(target, SIGTERM) < 0 && errno != ESRCH)
        return -1;

    int status = 0;
    for (;;) {
        pid_t waited = waitpid(target, &status, 0);
        if (waited == target)
            return 0;
        if (waited < 0 && errno == EINTR)
            continue;
        if (waited < 0 && errno == ECHILD)
            return 0;
        return -1;
    }
}

static void emergency_reboot(void)
{
    reboot(RB_AUTOBOOT);
    _exit(EXIT_FAILURE);
}

static void settle_hw_watchdog_trial_and_reboot(
    flip_optimizer_hw_watchdog_t *hw_watchdog,
    flip_optimizer_calibration_t *calibration,
    int transaction_ready,
    int candidate_exercised,
    int stress_ok,
    unsigned target_khz,
    unsigned candidate_uv)
{
    if (!hw_watchdog ||
        !hw_watchdog->touched ||
        !calibration ||
        !transaction_ready)
        emergency_reboot();

    if (hw_watchdog->armed &&
        flip_optimizer_hw_watchdog_ping(
            hw_watchdog) < 0) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: hardware watchdog heartbeat failed during settlement\n");
        emergency_reboot();
    }

    if (!stress_ok) {
        if (!candidate_exercised) {
            if (flip_optimizer_calibration_session_cancel_unapplied(
                    calibration) < 0) {
                fprintf(
                    stderr,
                    "OPTIMIZER_CANDIDATE: watchdog-era unapplied cancel failed\n");
                emergency_reboot();
            }

            printf(
                "OPTIMIZER_CANDIDATE: WATCHDOG_ABORTED_UNAPPLIED "
                "target_khz=%u candidate_uv=%u\n",
                target_khz,
                candidate_uv);
        } else {
            if (flip_optimizer_calibration_session_confirm_unstable(
                    calibration) < 0) {
                fprintf(
                    stderr,
                    "OPTIMIZER_CANDIDATE: watchdog-era unstable commit failed\n");
                emergency_reboot();
            }

            printf(
                "OPTIMIZER_CANDIDATE: LIVE_UNSTABLE "
                "target_khz=%u candidate_uv=%u\n",
                target_khz,
                candidate_uv);
        }
    } else {
        if (flip_optimizer_calibration_session_confirm_stable(
                calibration) < 0) {
            fprintf(
                stderr,
                "OPTIMIZER_CANDIDATE: watchdog-era stable commit failed\n");
            emergency_reboot();
        }

        printf(
            "OPTIMIZER_CANDIDATE: LIVE_ONCE_OK "
            "target_khz=%u candidate_uv=%u reboot_required=1\n",
            target_khz,
            candidate_uv);
    }

    fflush(stdout);

    if (hw_watchdog->armed &&
        flip_optimizer_hw_watchdog_ping(
            hw_watchdog) < 0) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: final hardware watchdog heartbeat failed\n");
        emergency_reboot();
    }

    /*
     * Stock CPU state has already been restored and background SD writers are
     * still quiesced. Flush only the durable trial settlement, then reboot
     * while TSADC remains routed to RK817. If reboot stalls, watchdog pings
     * stop and the hardware recovery path takes over.
     */
    sync();

    printf(
        "OPTIMIZER_CANDIDATE: WATCHDOG_SETTLED_REBOOT\n");
    fflush(stdout);

    emergency_reboot();
}

static void settle_hw_watchdog_power_cancel_and_reboot(
    flip_optimizer_hw_watchdog_t *hw_watchdog,
    flip_optimizer_calibration_t *calibration,
    int transaction_ready,
    unsigned target_khz,
    unsigned candidate_uv)
{
    if (!hw_watchdog ||
        !hw_watchdog->touched ||
        !calibration ||
        !transaction_ready)
        emergency_reboot();

    if (hw_watchdog->armed &&
        flip_optimizer_hw_watchdog_ping(
            hw_watchdog) < 0)
        emergency_reboot();

    if (flip_optimizer_calibration_session_cancel_unapplied(
            calibration) < 0) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: charger-cancel trial cancellation failed\n");
        emergency_reboot();
    }

    if (clear_ui_optimizer_session_files() < 0) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: charger-cancel session cleanup failed\n");
        emergency_reboot();
    }

    printf(
        "OPTIMIZER_CANDIDATE: CHARGER_CANCELLED "
        "target_khz=%u candidate_uv=%u reboot_to_frontend=1\n",
        target_khz,
        candidate_uv);
    fflush(stdout);

    if (hw_watchdog->armed &&
        flip_optimizer_hw_watchdog_ping(
            hw_watchdog) < 0)
        emergency_reboot();

    sync();
    emergency_reboot();
}

static int ensure_candidate_ready(
    flip_optimizer_calibration_t *calibration)
{
    if (!calibration)
        return -1;

    if (flip_optimizer_supervisor_runtime_stock_safe()) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: runtime stock-safe latch active\n");
        return -1;
    }

    if (flip_optimizer_calibration_load(
            calibration) < 0)
        return -1;

    if (calibration->pending) {
        return
            flip_optimizer_calibration_session_ready_for_apply(
                calibration)
            ? 0
            : -1;
    }

    int rc =
        flip_optimizer_calibration_session_prepare(
            calibration,
            NULL,
            NULL);

    return rc == 1 &&
        flip_optimizer_calibration_session_ready_for_apply(
            calibration)
        ? 0
        : -1;
}

static int run_table_only(void)
{
    if (geteuid() != 0) {
        fprintf(stderr, "OPTIMIZER_CANDIDATE: root required\n");
        return -1;
    }

    flip_optimizer_calibration_t calibration;
    int transaction_ready = 0;
    int operation_ok = 0;
    int original_valid = 0;
    int policy_modified = 0;
    int module_loaded = 0;
    int table_adjusted = 0;
    int cleanup_ok = 1;

    if (ensure_candidate_ready(&calibration) < 0) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: candidate/guard not ready\n");
        return -1;
    }

    transaction_ready = 1;

    size_t index = calibration.pending_index;
    const flip_optimizer_calibration_opp_t *opp =
        &calibration.opps[index];
    unsigned target_khz = opp->frequency_khz;
    unsigned candidate_uv = calibration.pending_uv;

    flip_cpu_policy_snapshot_t original;
    flip_cpu_policy_info_t info;

    if (flip_cpu_policy_snapshot(&original) < 0 ||
        flip_cpu_policy_query_info(&info) < 0) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: CPU policy snapshot failed\n");
        goto fail;
    }

    original_valid = 1;

    unsigned parking_khz = 0;

    if (choose_parking_frequency(
            &info,
            target_khz,
            &original,
            &parking_khz) < 0) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: no safe parking OPP\n");
        goto fail;
    }

    flip_opp_voltage_triplet_t stock;

    if (read_opp_triplet(target_khz, &stock) < 0 ||
        stock.target_uv != opp->stock_uv ||
        stock.min_uv > stock.target_uv ||
        stock.max_uv < stock.target_uv) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: stock OPP mismatch\n");
        goto fail;
    }

    flip_opp_voltage_triplet_t parking;
    if (read_opp_triplet(parking_khz, &parking) < 0) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: parking OPP read failed\n");
        goto fail;
    }

    policy_modified = 1;

    if (park_cpu(
            &original,
            target_khz,
            parking_khz) < 0) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: failed to park CPU\n");
        goto fail;
    }

    unsigned current = 0;
    unsigned rail_uv = 0;

    if (flip_cpu_policy_get_current(&current) < 0 ||
        current != parking_khz ||
        read_cpu_rail_uv(&rail_uv) < 0 ||
        rail_uv != parking.target_uv) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: parking verification failed "
            "freq=%u rail_uv=%u expected_freq=%u expected_uv=%u\n",
            current,
            rail_uv,
            parking_khz,
            parking.target_uv);
        goto fail;
    }

    if (load_module() < 0) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: module load failed errno=%d\n",
            errno);
        goto fail;
    }

    module_loaded = 1;

    if (!flip_optimizer_calibration_session_ready_for_apply(
            &calibration)) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: guard changed after parking\n");
        goto fail;
    }

    if (module_apply_table(
            target_khz,
            candidate_uv,
            &stock) < 0) {
        flip_opp_voltage_triplet_t current_triplet;

        if (read_opp_triplet(
                target_khz,
                &current_triplet) == 0 &&
            !triplet_matches(
                &current_triplet,
                &stock))
            table_adjusted = 1;

        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: OPP table apply failed errno=%d\n",
            errno);
        goto fail;
    }

    table_adjusted = 1;

    flip_opp_voltage_triplet_t adjusted;
    flip_opp_voltage_triplet_t expected = {
        .target_uv = candidate_uv,
        .min_uv = candidate_uv,
        .max_uv = candidate_uv,
    };

    if (read_opp_triplet(
            target_khz,
            &adjusted) < 0 ||
        !triplet_matches(
            &adjusted,
            &expected) ||
        flip_cpu_policy_get_current(&current) < 0 ||
        current != parking_khz ||
        read_cpu_rail_uv(&rail_uv) < 0 ||
        rail_uv != parking.target_uv) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: adjusted-table verification failed\n");
        goto fail;
    }

    operation_ok = 1;

    printf(
        "OPTIMIZER_CANDIDATE: TABLE_ONLY_OK "
        "trial=%s target_khz=%u candidate_uv=%u "
        "park_khz=%u park_uv=%u voltage_entered=0\n",
        calibration.pending_trial_id,
        target_khz,
        candidate_uv,
        parking_khz,
        parking.target_uv);

fail:
    if (table_adjusted) {
        if (module_restore_table() < 0)
            cleanup_ok = 0;
        else {
            flip_opp_voltage_triplet_t restored;

            if (read_opp_triplet(
                    target_khz,
                    &restored) < 0 ||
                !triplet_matches(
                    &restored,
                    &stock))
                cleanup_ok = 0;
            else
                table_adjusted = 0;
        }
    }

    if (!cleanup_ok) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: table restore failed; rebooting stock\n");
        emergency_reboot();
    }

    if (module_loaded && unload_module() < 0) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: module unload failed; rebooting\n");
        emergency_reboot();
    }

    if (policy_modified &&
        original_valid &&
        flip_cpu_policy_restore(&original) < 0) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: CPU policy restore failed; rebooting\n");
        emergency_reboot();
    }

    if (table_adjusted)
        emergency_reboot();

    /*
     * A table-only roundtrip never ran silicon at the candidate voltage, so
     * it must not count as stable or unstable calibration evidence.
     */
    if (transaction_ready &&
        flip_optimizer_calibration_session_cancel_unapplied(
            &calibration) < 0) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: trial cancellation failed; rebooting\n");
        emergency_reboot();
    }

    /*
     * Success is only true if the table was actually adjusted and verified.
     * Detect it from the printed path by re-checking that the stock table and
     * original policy are now restored.
     */
    if (!operation_ok)
        return -1;

    flip_opp_voltage_triplet_t final_triplet;
    flip_cpu_policy_snapshot_t final_policy;

    if (!original_valid ||
        read_opp_triplet(
            target_khz,
            &final_triplet) < 0 ||
        !triplet_matches(
            &final_triplet,
            &stock) ||
        flip_cpu_policy_snapshot(&final_policy) < 0 ||
        final_policy.min_khz != original.min_khz ||
        final_policy.max_khz != original.max_khz ||
        strcmp(
            final_policy.governor,
            original.governor) != 0)
        return -1;

    return 0;
}

static int run_live_once(unsigned duration_seconds)
{
    if (geteuid() != 0) {
        fprintf(stderr, "OPTIMIZER_CANDIDATE: root required\n");
        return -1;
    }

    if (duration_seconds <
            FLIP_OPTIMIZER_STRESS_MIN_SECONDS ||
        duration_seconds >
            FLIP_OPTIMIZER_STRESS_MAX_SECONDS) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: live duration must be %u-%u seconds\n",
            FLIP_OPTIMIZER_STRESS_MIN_SECONDS,
            FLIP_OPTIMIZER_STRESS_MAX_SECONDS);
        return -1;
    }

    flip_optimizer_calibration_t calibration;
    int transaction_ready = 0;
    int original_valid = 0;
    int policy_modified = 0;
    int module_loaded = 0;
    int table_adjusted = 0;
    int voltage_entered = 0;
    int cleanup_ok = 1;
    int stress_ok = 0;
    int candidate_exercised = 0;
    int power_cancelled = 0;
    int require_external_power =
        optimizer_ui_power_interlock_active();
    int tsadc_module_loaded = 0;
    int tsadc_recovery_armed = 0;
    flip_optimizer_quiesce_t quiesce = {0};
    flip_optimizer_panic_policy_t panic_policy = {0};
    pid_t deadman_pid = -1;
    flip_optimizer_hw_watchdog_t hw_watchdog = {
        .fd = -1,
    };

    if (ensure_candidate_ready(&calibration) < 0) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: candidate/guard not ready\n");
        return -1;
    }

    transaction_ready = 1;

    size_t index = calibration.pending_index;
    const flip_optimizer_calibration_opp_t *opp =
        &calibration.opps[index];
    unsigned target_khz = opp->frequency_khz;
    unsigned candidate_uv = calibration.pending_uv;

    flip_cpu_policy_snapshot_t original;
    flip_cpu_policy_info_t info;

    if (flip_cpu_policy_snapshot(&original) < 0 ||
        flip_cpu_policy_query_info(&info) < 0) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: CPU policy snapshot failed\n");
        goto fail;
    }

    original_valid = 1;

    unsigned parking_khz = 0;

    if (choose_parking_frequency(
            &info,
            target_khz,
            &original,
            &parking_khz) < 0) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: no safe parking OPP\n");
        goto fail;
    }

    flip_opp_voltage_triplet_t stock;
    flip_opp_voltage_triplet_t parking;

    if (read_opp_triplet(target_khz, &stock) < 0 ||
        stock.target_uv != opp->stock_uv ||
        stock.min_uv > stock.target_uv ||
        stock.max_uv < stock.target_uv ||
        read_opp_triplet(parking_khz, &parking) < 0) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: OPP preflight failed\n");
        goto fail;
    }

    policy_modified = 1;

    if (park_cpu(
            &original,
            target_khz,
            parking_khz) < 0) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: failed to park CPU\n");
        goto fail;
    }

    unsigned current = 0;
    unsigned rail_uv = 0;

    if (flip_cpu_policy_get_current(&current) < 0 ||
        current != parking_khz ||
        read_cpu_rail_uv(&rail_uv) < 0 ||
        rail_uv != parking.target_uv) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: parking verification failed "
            "freq=%u rail_uv=%u expected_freq=%u expected_uv=%u\n",
            current,
            rail_uv,
            parking_khz,
            parking.target_uv);
        goto fail;
    }

    if (load_module() < 0) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: module load failed errno=%d\n",
            errno);
        goto fail;
    }

    module_loaded = 1;

    if (!flip_optimizer_calibration_session_ready_for_apply(
            &calibration)) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: guard changed after parking\n");
        goto fail;
    }

    if (module_apply_table(
            target_khz,
            candidate_uv,
            &stock) < 0) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: OPP table apply failed errno=%d\n",
            errno);
        goto fail;
    }

    table_adjusted = 1;

    flip_opp_voltage_triplet_t adjusted;
    flip_opp_voltage_triplet_t expected = {
        .target_uv = candidate_uv,
        .min_uv = candidate_uv,
        .max_uv = candidate_uv,
    };

    if (read_opp_triplet(
            target_khz,
            &adjusted) < 0 ||
        !triplet_matches(&adjusted, &expected) ||
        flip_cpu_policy_get_current(&current) < 0 ||
        current != parking_khz ||
        read_cpu_rail_uv(&rail_uv) < 0 ||
        rail_uv != parking.target_uv) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: adjusted-table verification failed\n");
        goto fail;
    }

    if (flip_optimizer_quiesce_begin(
            &quiesce) < 0) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: failed to quiesce SD writers\n");
        goto fail;
    }

    printf(
        "OPTIMIZER_CANDIDATE: storage quiesced writers=%zu\n",
        quiesce.count);
    fflush(stdout);

    if (flip_optimizer_panic_policy_begin(
            &panic_policy) < 0) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: failed to arm kernel recovery policy\n");
        goto fail;
    }

    printf(
        "OPTIMIZER_CANDIDATE: kernel recovery policy armed\n");
    fflush(stdout);

    if (tsadc_recovery_load() < 0) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: failed to load TSADC recovery route\n");
        goto fail;
    }
    tsadc_module_loaded = 1;

    if (tsadc_recovery_set(1) < 0) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: failed to arm TSADC PMIC recovery route\n");
        goto fail;
    }
    tsadc_recovery_armed = 1;

    printf(
        "OPTIMIZER_CANDIDATE: TSADC PMIC recovery route armed\n");
    fflush(stdout);

    if (flip_optimizer_hw_watchdog_arm(
            &hw_watchdog,
            FLIP_OPTIMIZER_HW_WATCHDOG_REQUEST_SECONDS) < 0) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: hardware watchdog arm failed touched=%d\n",
            hw_watchdog.touched);
        goto fail;
    }

    printf(
        "OPTIMIZER_CANDIDATE: hardware watchdog armed timeout_s=%u\n",
        hw_watchdog.timeout_seconds);
    fflush(stdout);

    unsigned deadman_timeout =
        duration_seconds +
        FLIP_OPTIMIZER_DEADMAN_HEADROOM_SECONDS;

    if (arm_deadman(
            deadman_timeout,
            &deadman_pid) < 0) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: failed to arm recovery deadman\n");
        goto fail;
    }

    printf(
        "OPTIMIZER_CANDIDATE: deadman armed timeout_s=%u pid=%ld\n",
        deadman_timeout,
        (long)deadman_pid);
    fflush(stdout);

    if (flip_optimizer_guard_mark_entering(
            calibration.pending_trial_id) < 0) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: failed to mark PMU entering phase\n");
        goto fail;
    }

    printf(
        "OPTIMIZER_CANDIDATE: PMU_ENTERING trial=%s\n",
        calibration.pending_trial_id);
    fflush(stdout);

    if (enter_candidate_frequency(
            &original,
            target_khz,
            parking_khz) < 0 ||
        flip_cpu_policy_get_current(&current) < 0 ||
        current != target_khz ||
        read_cpu_rail_uv(&rail_uv) < 0 ||
        rail_uv != candidate_uv) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: failed to enter candidate "
            "freq=%u/%u rail_uv=%u/%u\n",
            current,
            target_khz,
            rail_uv,
            candidate_uv);
        goto fail;
    }

    voltage_entered = 1;
    candidate_exercised = 1;

    if (run_integrity_check() < 0) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: pre-stress integrity failed\n");
        goto fail;
    }

    printf(
        "OPTIMIZER_CANDIDATE: LIVE_INTEGRITY_OK phase=pre\n");
    fflush(stdout);

    unsigned max_temp_mc = 0;
    unsigned samples = 0;

    printf(
        "OPTIMIZER_CANDIDATE: LIVE_BEGIN "
        "trial=%s target_khz=%u candidate_uv=%u "
        "duration_s=%u\n",
        calibration.pending_trial_id,
        target_khz,
        candidate_uv,
        duration_seconds);
    fflush(stdout);

    if (run_stress_window(
            duration_seconds,
            target_khz,
            candidate_uv,
            &hw_watchdog,
            require_external_power,
            &power_cancelled,
            &max_temp_mc,
            &samples) < 0) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: live stress failed\n");
        goto fail;
    }

    if (power_cancelled) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: charger removal detected during live trial\n");
        goto fail;
    }

    if (run_integrity_check() < 0) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: post-stress integrity failed\n");
        goto fail;
    }

    printf(
        "OPTIMIZER_CANDIDATE: LIVE_INTEGRITY_OK phase=post\n");
    fflush(stdout);

    stress_ok = 1;

    printf(
        "OPTIMIZER_CANDIDATE: LIVE_STRESS_OK "
        "trial=%s samples=%u max_temp_mc=%u\n",
        calibration.pending_trial_id,
        samples,
        max_temp_mc);
    fflush(stdout);

fail:
    if (voltage_entered) {
        if (return_to_parking_frequency(parking_khz) < 0 ||
            flip_cpu_policy_get_current(&current) < 0 ||
            current != parking_khz ||
            read_cpu_rail_uv(&rail_uv) < 0 ||
            rail_uv != parking.target_uv) {
            cleanup_ok = 0;
        } else {
            voltage_entered = 0;
        }
    }

    if (table_adjusted) {
        if (module_restore_table() < 0) {
            cleanup_ok = 0;
        } else {
            flip_opp_voltage_triplet_t restored;

            if (read_opp_triplet(
                    target_khz,
                    &restored) < 0 ||
                !triplet_matches(
                    &restored,
                    &stock))
                cleanup_ok = 0;
            else
                table_adjusted = 0;
        }
    }

    if (!cleanup_ok ||
        voltage_entered ||
        table_adjusted) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: live cleanup unsafe; rebooting stock\n");
        emergency_reboot();
    }

    if (module_loaded && unload_module() < 0) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: module unload failed; rebooting\n");
        emergency_reboot();
    }

    if (policy_modified &&
        original_valid &&
        flip_cpu_policy_restore(&original) < 0) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: CPU policy restore failed; rebooting\n");
        emergency_reboot();
    }

    if (disarm_deadman(&deadman_pid) < 0) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: recovery deadman disarm failed; rebooting\n");
        emergency_reboot();
    }

    if (require_external_power &&
        optimizer_external_power_online() <= 0)
        power_cancelled = 1;

    if (hw_watchdog.touched) {
        if (power_cancelled) {
            settle_hw_watchdog_power_cancel_and_reboot(
                &hw_watchdog,
                &calibration,
                transaction_ready,
                target_khz,
                candidate_uv);
        }

        settle_hw_watchdog_trial_and_reboot(
            &hw_watchdog,
            &calibration,
            transaction_ready,
            candidate_exercised,
            stress_ok,
            target_khz,
            candidate_uv);
    }

    if (tsadc_recovery_armed) {
        if (tsadc_recovery_set(0) < 0) {
            fprintf(
                stderr,
                "OPTIMIZER_CANDIDATE: failed to restore stock TSADC route; rebooting\n");
            emergency_reboot();
        }
        tsadc_recovery_armed = 0;
    }

    if (tsadc_module_loaded) {
        if (tsadc_recovery_unload() < 0) {
            fprintf(
                stderr,
                "OPTIMIZER_CANDIDATE: failed to unload TSADC recovery module; rebooting\n");
            emergency_reboot();
        }
        tsadc_module_loaded = 0;
    }

    if (flip_optimizer_panic_policy_end(
            &panic_policy) < 0) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: failed to restore kernel recovery policy; rebooting\n");
        emergency_reboot();
    }

    if (flip_optimizer_quiesce_end(&quiesce) < 0) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: failed to resume SD writers; rebooting\n");
        emergency_reboot();
    }

    printf(
        "OPTIMIZER_CANDIDATE: storage writers resumed\n");
    fflush(stdout);

    if (!stress_ok) {
        if (!transaction_ready)
            return -1;

        if (!candidate_exercised) {
            if (flip_optimizer_calibration_session_cancel_unapplied(
                    &calibration) < 0) {
                fprintf(
                    stderr,
                    "OPTIMIZER_CANDIDATE: unapplied trial cancel failed; "
                    "rebooting\n");
                emergency_reboot();
            }

            fprintf(
                stderr,
                "OPTIMIZER_CANDIDATE: live attempt aborted before "
                "candidate entry\n");
            return -1;
        }

        if (flip_optimizer_calibration_session_confirm_unstable(
                &calibration) < 0) {
            fprintf(
                stderr,
                "OPTIMIZER_CANDIDATE: unstable result commit failed; "
                "rebooting\n");
            emergency_reboot();
        }

        printf(
            "OPTIMIZER_CANDIDATE: LIVE_UNSTABLE "
            "target_khz=%u candidate_uv=%u\n",
            target_khz,
            candidate_uv);
        return 0;
    }

    if (!transaction_ready ||
        flip_optimizer_calibration_session_confirm_stable(
            &calibration) < 0) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: confirmation failed; rebooting\n");
        emergency_reboot();
    }

    flip_cpu_policy_snapshot_t final_policy;
    flip_opp_voltage_triplet_t final_triplet;

    if (!original_valid ||
        flip_cpu_policy_snapshot(&final_policy) < 0 ||
        final_policy.min_khz != original.min_khz ||
        final_policy.max_khz != original.max_khz ||
        strcmp(
            final_policy.governor,
            original.governor) != 0 ||
        read_opp_triplet(
            target_khz,
            &final_triplet) < 0 ||
        !triplet_matches(
            &final_triplet,
            &stock)) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: post-confirm stock verification failed\n");
        return -1;
    }

    printf(
        "OPTIMIZER_CANDIDATE: LIVE_ONCE_OK "
        "target_khz=%u candidate_uv=%u duration_s=%u\n",
        target_khz,
        candidate_uv,
        duration_seconds);

    return 0;
}

static int run_quiesce_probe(unsigned seconds)
{
    if (geteuid() != 0) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: root required\n");
        return -1;
    }

    if (seconds == 0 || seconds > 30) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: quiesce probe duration must be 1-30 seconds\n");
        return -1;
    }

    flip_optimizer_quiesce_t quiesce = {0};

    if (flip_optimizer_quiesce_begin(&quiesce) < 0) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: quiesce probe begin failed\n");
        return -1;
    }

    printf(
        "OPTIMIZER_CANDIDATE: QUIESCE_PROBE_BEGIN writers=%zu",
        quiesce.count);

    for (size_t i = 0; i < quiesce.count; ++i)
        printf(" pid%zu=%ld", i, (long)quiesce.pids[i]);

    printf("\n");
    fflush(stdout);

    struct timespec remaining = {
        .tv_sec = (time_t)seconds,
        .tv_nsec = 0,
    };

    while (nanosleep(&remaining, &remaining) < 0) {
        if (errno == EINTR)
            continue;

        (void)flip_optimizer_quiesce_end(&quiesce);
        return -1;
    }

    if (flip_optimizer_quiesce_end(&quiesce) < 0)
        return -1;

    printf(
        "OPTIMIZER_CANDIDATE: QUIESCE_PROBE_OK seconds=%u\n",
        seconds);
    return 0;
}

static int wait_for_a_press(unsigned timeout_seconds)
{
    int fd = open(
        "/dev/input/event0",
        O_RDONLY | O_NONBLOCK | O_CLOEXEC);

    if (fd < 0)
        return -1;

    struct timespec start;
    if (clock_gettime(CLOCK_MONOTONIC, &start) < 0) {
        close(fd);
        return -1;
    }

    for (;;) {
        struct input_event ev;
        ssize_t n = read(fd, &ev, sizeof(ev));

        if (n == (ssize_t)sizeof(ev)) {
            if (ev.type == EV_KEY &&
                ev.code == 57 &&
                ev.value == 1) {
                close(fd);
                return 1;
            }
            continue;
        }

        if (n < 0 &&
            errno != EAGAIN &&
            errno != EWOULDBLOCK &&
            errno != EINTR) {
            close(fd);
            return -1;
        }

        struct timespec now;
        if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) {
            close(fd);
            return -1;
        }

        if ((unsigned)(now.tv_sec - start.tv_sec) >=
            timeout_seconds) {
            close(fd);
            return 0;
        }

        const struct timespec delay = {
            .tv_sec = 0,
            .tv_nsec = 20 * 1000 * 1000,
        };
        (void)nanosleep(&delay, NULL);
    }
}

static int run_watchdog_button_expiry_probe(void)
{
    if (geteuid() != 0) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: root required\n");
        return -1;
    }

    flip_optimizer_quiesce_t quiesce = {0};
    flip_optimizer_hw_watchdog_t hw_watchdog = {
        .fd = -1,
    };
    int tsadc_loaded = 0;
    int tsadc_armed = 0;
    int pressed = -1;

    if (flip_optimizer_quiesce_begin(
            &quiesce) < 0)
        return -1;

    if (tsadc_recovery_load() < 0)
        goto safe_abort;
    tsadc_loaded = 1;

    if (tsadc_recovery_set(1) < 0)
        goto safe_abort;
    tsadc_armed = 1;

    printf(
        "OPTIMIZER_CANDIDATE: WATCHDOG_BUTTON_PROBE_READY "
        "press_A=1 timeout_s=60 voltage_written=0\n");
    fflush(stdout);

    pressed = wait_for_a_press(60);

    if (pressed <= 0) {
        if (pressed == 0) {
            printf(
                "OPTIMIZER_CANDIDATE: WATCHDOG_BUTTON_PROBE_TIMEOUT\n");
            fflush(stdout);
        }
        goto safe_abort;
    }

    if (flip_optimizer_hw_watchdog_arm(
            &hw_watchdog,
            FLIP_OPTIMIZER_HW_WATCHDOG_REQUEST_SECONDS) < 0) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: watchdog button probe arm failed touched=%d\n",
            hw_watchdog.touched);

        if (hw_watchdog.touched)
            emergency_reboot();

        goto safe_abort;
    }

    sync();

    printf(
        "OPTIMIZER_CANDIDATE: WATCHDOG_BUTTON_PROBE_TRIGGERED "
        "timeout_s=%u voltage_written=0 stopping_heartbeats=1\n",
        hw_watchdog.timeout_seconds);
    fflush(stdout);

    for (;;)
        pause();

safe_abort:
    if (tsadc_armed)
        (void)tsadc_recovery_set(0);
    if (tsadc_loaded)
        (void)tsadc_recovery_unload();
    (void)flip_optimizer_quiesce_end(&quiesce);
    return pressed < 0 ? -1 : 0;
}

static int run_watchdog_reboot_probe(void)
{
    if (geteuid() != 0) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: root required\n");
        return -1;
    }

    flip_optimizer_quiesce_t quiesce = {0};
    flip_optimizer_hw_watchdog_t hw_watchdog = {
        .fd = -1,
    };
    int tsadc_loaded = 0;
    int tsadc_armed = 0;

    if (flip_optimizer_quiesce_begin(
            &quiesce) < 0) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: watchdog reboot probe quiesce failed\n");
        return -1;
    }

    if (tsadc_recovery_load() < 0) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: watchdog reboot probe TSADC load failed\n");
        goto fail_before_watchdog;
    }
    tsadc_loaded = 1;

    if (tsadc_recovery_set(1) < 0) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: watchdog reboot probe TSADC arm failed\n");
        goto fail_before_watchdog;
    }
    tsadc_armed = 1;

    if (flip_optimizer_hw_watchdog_arm(
            &hw_watchdog,
            FLIP_OPTIMIZER_HW_WATCHDOG_REQUEST_SECONDS) < 0) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: watchdog reboot probe arm failed touched=%d\n",
            hw_watchdog.touched);

        if (hw_watchdog.touched)
            emergency_reboot();

        goto fail_before_watchdog;
    }

    printf(
        "OPTIMIZER_CANDIDATE: WATCHDOG_REBOOT_PROBE_ARMED "
        "timeout_s=%u writers=%zu voltage_written=0\n",
        hw_watchdog.timeout_seconds,
        quiesce.count);
    fflush(stdout);

    for (unsigned i = 0; i < 10; ++i) {
        const struct timespec delay = {
            .tv_sec = 0,
            .tv_nsec = 200 * 1000 * 1000,
        };

        if (flip_optimizer_hw_watchdog_ping(
                &hw_watchdog) < 0)
            emergency_reboot();

        if (nanosleep(&delay, NULL) < 0 &&
            errno != EINTR)
            emergency_reboot();
    }

    if (flip_optimizer_hw_watchdog_ping(
            &hw_watchdog) < 0)
        emergency_reboot();

    sync();

    printf(
        "OPTIMIZER_CANDIDATE: WATCHDOG_REBOOT_PROBE_REBOOTING\n");
    fflush(stdout);

    /*
     * Do not restore TSADC or close the hardware watchdog. The acceptance
     * condition is that normal reboot succeeds while the fallback remains
     * armed; if reboot stalls, missing watchdog heartbeats provide fallback.
     */
    emergency_reboot();

fail_before_watchdog:
    if (tsadc_armed)
        (void)tsadc_recovery_set(0);
    if (tsadc_loaded)
        (void)tsadc_recovery_unload();
    (void)flip_optimizer_quiesce_end(&quiesce);
    return -1;
}

static int run_kernel_recovery_probe(unsigned seconds)
{
    if (geteuid() != 0) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: root required\n");
        return -1;
    }

    if (seconds == 0 || seconds > 30) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: kernel recovery probe duration must be 1-30 seconds\n");
        return -1;
    }

    flip_optimizer_panic_policy_t policy = {0};

    if (flip_optimizer_panic_policy_begin(
            &policy) < 0) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: kernel recovery probe begin failed\n");
        return -1;
    }

    printf(
        "OPTIMIZER_CANDIDATE: KERNEL_RECOVERY_PROBE_BEGIN "
        "seconds=%u original_panic=%d original_oops=%d "
        "original_warn=%d original_rcu=%d\n",
        seconds,
        policy.panic_seconds,
        policy.panic_on_oops,
        policy.panic_on_warn,
        policy.panic_on_rcu_stall);
    fflush(stdout);

    struct timespec remaining = {
        .tv_sec = (time_t)seconds,
        .tv_nsec = 0,
    };

    while (nanosleep(&remaining, &remaining) < 0) {
        if (errno == EINTR)
            continue;

        (void)flip_optimizer_panic_policy_end(
            &policy);
        return -1;
    }

    if (flip_optimizer_panic_policy_end(
            &policy) < 0)
        return -1;

    printf(
        "OPTIMIZER_CANDIDATE: KERNEL_RECOVERY_PROBE_OK seconds=%u\n",
        seconds);
    return 0;
}

static int run_kernel_recovery_crash_test(void)
{
    if (geteuid() != 0) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: root required\n");
        return -1;
    }

    flip_optimizer_quiesce_t quiesce = {0};
    flip_optimizer_panic_policy_t policy = {0};

    if (flip_optimizer_quiesce_begin(
            &quiesce) < 0) {
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: crash-test quiesce failed\n");
        return -1;
    }

    if (flip_optimizer_panic_policy_begin(
            &policy) < 0) {
        (void)flip_optimizer_quiesce_end(&quiesce);
        fprintf(
            stderr,
            "OPTIMIZER_CANDIDATE: crash-test recovery policy failed\n");
        return -1;
    }

    printf(
        "OPTIMIZER_CANDIDATE: KERNEL_RECOVERY_CRASH_TEST_ARMED "
        "writers=%zu reboot_after_s=10 voltage_written=0\n",
        quiesce.count);
    fflush(stdout);

    FILE *trigger = fopen("/proc/sysrq-trigger", "w");
    if (!trigger) {
        (void)flip_optimizer_panic_policy_end(&policy);
        (void)flip_optimizer_quiesce_end(&quiesce);
        return -1;
    }

    int failed =
        fputs("c\n", trigger) == EOF ||
        fflush(trigger) != 0;

    if (failed || fclose(trigger) != 0) {
        (void)flip_optimizer_panic_policy_end(&policy);
        (void)flip_optimizer_quiesce_end(&quiesce);
        return -1;
    }

    /*
     * A successful SysRq crash never returns. If it does, restore everything
     * and fail rather than leaving the normal OS in test policy.
     */
    (void)flip_optimizer_panic_policy_end(&policy);
    (void)flip_optimizer_quiesce_end(&quiesce);
    fprintf(
        stderr,
        "OPTIMIZER_CANDIDATE: crash trigger unexpectedly returned\n");
    return -1;
}

static int run_child_tool(const char *tool, const char *arg)
{
    pid_t pid = fork();
    if (pid < 0)
        return -1;

    if (pid == 0) {
        execl(tool, tool, arg, (char *)NULL);
        _exit(127);
    }

    int status = 0;
    for (;;) {
        if (waitpid(pid, &status, 0) >= 0)
            break;
        if (errno != EINTR)
            return -1;
    }

    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -1;
}

static int run_child_tool_with_watchdog(
    const char *tool,
    const char *arg,
    flip_optimizer_hw_watchdog_t *hw_watchdog)
{
    if (!tool || !arg || !hw_watchdog || !hw_watchdog->armed)
        return -1;

    pid_t pid = fork();
    if (pid < 0)
        return -1;

    if (pid == 0) {
        execl(tool, tool, arg, (char *)NULL);
        _exit(127);
    }

    const struct timespec delay = {
        .tv_sec = 0,
        .tv_nsec = 100 * 1000 * 1000,
    };

    int status = 0;
    for (;;) {
        pid_t rc = waitpid(pid, &status, WNOHANG);
        if (rc == pid)
            break;
        if (rc < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }

        if (flip_optimizer_hw_watchdog_ping(hw_watchdog) < 0)
            emergency_reboot();

        nanosleep(&delay, NULL);
    }

    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -1;
}

static int write_validation_state_file(
    const char *path,
    const char *status,
    const flip_optimizer_result_t *result,
    unsigned tested_opps,
    unsigned seconds_per_opp,
    unsigned max_temp_mc)
{
    if (!path || !status || !result)
        return -1;

    char temp[512];
    int n = snprintf(temp, sizeof(temp), "%s.tmp", path);
    if (n < 0 || (size_t)n >= sizeof(temp))
        return -1;

    FILE *f = fopen(temp, "w");
    if (!f)
        return -1;

    time_t now = time(NULL);
    unsigned epoch =
        now > 0 && (unsigned long long)now <= 0xffffffffULL
            ? (unsigned)now
            : 0;

    int failed = fprintf(
        f,
        "version=1\nstatus=%s\ndevice_id=%s\ncalibrated_epoch=%u\n"
        "tested_epoch=%u\ntested_opps=%u\nseconds_per_opp=%u\nmax_temp_mc=%u\n",
        status,
        result->device_id,
        result->calibrated_epoch,
        epoch,
        tested_opps,
        seconds_per_opp,
        max_temp_mc) < 0;

    if (!failed && fflush(f) != 0)
        failed = 1;
    if (!failed && fsync(fileno(f)) != 0)
        failed = 1;
    if (fclose(f) != 0)
        failed = 1;

    if (failed || rename(temp, path) < 0) {
        (void)unlink(temp);
        return -1;
    }

    sync();
    return 0;
}

static int validation_preconditions(
    flip_optimizer_result_t *result,
    unsigned *testable_opps)
{
    flip_optimizer_guard_status_t guard;

    if (!result || !testable_opps ||
        geteuid() != 0 ||
        optimizer_external_power_online() <= 0 ||
        access(FLIP_OPTIMIZER_VALIDATION_REQUEST, F_OK) != 0 ||
        access(FLIP_OPTIMIZER_UI_RETURN_FILE, F_OK) == 0 ||
        access(FLIP_OPTIMIZER_AUTO_SESSION_FILE, F_OK) == 0 ||
        flip_optimizer_guard_status(&guard) < 0 ||
        guard.active || guard.stock_only ||
        flip_optimizer_load(result) < 0)
        return -1;

    unsigned count = 0;
    for (size_t i = 0; i < result->opp_count; ++i) {
        const flip_optimizer_opp_t *opp = &result->opps[i];
        if (opp->frequency_khz >=
                FLIP_OPTIMIZER_CALIBRATION_RK3566_MIN_TUNABLE_KHZ &&
            opp->optimized_uv <= opp->stock_uv)
            count++;
    }

    if (!count)
        return -1;

    *testable_opps = count;
    return 0;
}

static int run_validate_profile(unsigned seconds_per_opp)
{
    if (seconds_per_opp < 10 || seconds_per_opp > 300)
        return -1;

    flip_optimizer_result_t result;
    unsigned total_opps = 0;

    if (validation_preconditions(&result, &total_opps) < 0) {
        fprintf(stderr, "OPTIMIZER_VALIDATION: preconditions failed\n");
        return -1;
    }

    if (write_validation_state_file(
            FLIP_OPTIMIZER_VALIDATION_ACTIVE,
            "running",
            &result,
            0,
            seconds_per_opp,
            0) < 0)
        return -1;

    flip_cpu_policy_snapshot_t original;
    int original_valid =
        flip_cpu_policy_snapshot(&original) == 0;
    int runtime_applied = 0;

    /*
     * Keep the CPU on the stock-preserved 408 MHz OPP while the optimized
     * table is installed and until the full recovery envelope is armed.
     * Never expose the normal governor to a newly applied runtime table
     * before watchdog/TSADC/deadman protection exists.
     */
    if (!original_valid ||
        flip_cpu_policy_set_limits(
            FLIP_OPTIMIZER_VALIDATION_PARK_KHZ,
            FLIP_OPTIMIZER_VALIDATION_PARK_KHZ) < 0 ||
        wait_for_frequency(
            FLIP_OPTIMIZER_VALIDATION_PARK_KHZ) < 0) {
        if (original_valid)
            (void)flip_cpu_policy_restore(&original);
        (void)write_validation_state_file(
            FLIP_OPTIMIZER_VALIDATION_RESULT,
            "start-failed",
            &result,
            0,
            seconds_per_opp,
            0);
        (void)unlink(FLIP_OPTIMIZER_VALIDATION_ACTIVE);
        (void)unlink(FLIP_OPTIMIZER_VALIDATION_REQUEST);
        sync();
        return -1;
    }
    flip_optimizer_quiesce_t quiesce = {0};
    flip_optimizer_panic_policy_t panic_policy = {0};
    flip_optimizer_hw_watchdog_t hw_watchdog = {.fd = -1};
    pid_t deadman_pid = -1;
    int quiesced = 0;
    int panic_armed = 0;
    int tsadc_loaded = 0;
    int tsadc_armed = 0;
    int power_cancelled = 0;
    int failed = 0;
    unsigned tested_opps = 0;
    unsigned global_max_temp = 0;

    if (!original_valid ||
        flip_optimizer_quiesce_begin(&quiesce) < 0)
        failed = 1;
    else
        quiesced = 1;

    if (!failed && flip_optimizer_panic_policy_begin(&panic_policy) < 0)
        failed = 1;
    else if (!failed)
        panic_armed = 1;

    if (!failed && tsadc_recovery_load() < 0)
        failed = 1;
    else if (!failed)
        tsadc_loaded = 1;

    if (!failed && tsadc_recovery_set(1) < 0)
        failed = 1;
    else if (!failed)
        tsadc_armed = 1;

    if (failed)
        goto validation_abort_before_watchdog;

    if (flip_optimizer_hw_watchdog_arm(
            &hw_watchdog,
            FLIP_OPTIMIZER_HW_WATCHDOG_REQUEST_SECONDS) < 0) {
        if (hw_watchdog.touched)
            emergency_reboot();
        failed = 1;
        goto validation_abort_before_watchdog;
    }

    unsigned total_seconds = seconds_per_opp * total_opps;
    if (arm_deadman(
            total_seconds + FLIP_OPTIMIZER_DEADMAN_HEADROOM_SECONDS,
            &deadman_pid) < 0)
        emergency_reboot();

    /*
     * Only expose the optimized OPP table after the complete recovery
     * envelope is armed. validation-active is already durable, so the
     * validation path deliberately does not create the gameplay runtime
     * active marker. A reboot with validation-active still present is
     * sufficient evidence of a hard validation recovery.
     */
    if (write_validation_state_file(
            FLIP_OPTIMIZER_VALIDATION_ACTIVE,
            "applying-profile",
            &result,
            0,
            seconds_per_opp,
            0) < 0)
        emergency_reboot();

    if (run_child_tool_with_watchdog(
            FLIP_OPTIMIZER_RUNTIME_TOOL,
            "apply-validation",
            &hw_watchdog) < 0)
        emergency_reboot();
    runtime_applied = 1;

    if (flip_optimizer_hw_watchdog_ping(&hw_watchdog) < 0)
        emergency_reboot();

    if (write_validation_state_file(
            FLIP_OPTIMIZER_VALIDATION_ACTIVE,
            "profile-applied",
            &result,
            0,
            seconds_per_opp,
            0) < 0)
        emergency_reboot();

    printf(
        "OPTIMIZER_VALIDATION: BEGIN opps=%u seconds_per_opp=%u\n",
        total_opps,
        seconds_per_opp);
    fflush(stdout);

    for (size_t reverse = result.opp_count;
         !failed && reverse > 0;
         --reverse) {
        const flip_optimizer_opp_t *opp = &result.opps[reverse - 1];

        if (opp->frequency_khz <
                FLIP_OPTIMIZER_CALIBRATION_RK3566_MIN_TUNABLE_KHZ)
            continue;

        if (optimizer_external_power_online() <= 0) {
            power_cancelled = 1;
            break;
        }

        char validation_stage[64];
        int stage_n = snprintf(
            validation_stage,
            sizeof(validation_stage),
            "entering-%u",
            opp->frequency_khz);
        if (stage_n < 0 ||
            (size_t)stage_n >= sizeof(validation_stage) ||
            write_validation_state_file(
                FLIP_OPTIMIZER_VALIDATION_ACTIVE,
                validation_stage,
                &result,
                tested_opps,
                seconds_per_opp,
                global_max_temp) < 0)
            emergency_reboot();

        if (flip_optimizer_hw_watchdog_ping(&hw_watchdog) < 0)
            emergency_reboot();

        if (flip_cpu_policy_set_limits(
                opp->frequency_khz,
                opp->frequency_khz) < 0) {
            failed = 1;
            break;
        }

        unsigned current = 0;
        unsigned rail_uv = 0;
        if (flip_cpu_policy_get_current(&current) < 0 ||
            current != opp->frequency_khz ||
            read_cpu_rail_uv(&rail_uv) < 0 ||
            rail_uv != opp->optimized_uv) {
            fprintf(
                stderr,
                "OPTIMIZER_VALIDATION: entry verification failed "
                "freq=%u/%u rail_uv=%u/%u\n",
                current,
                opp->frequency_khz,
                rail_uv,
                opp->optimized_uv);
            failed = 1;
            break;
        }

        if (run_integrity_check() < 0) {
            failed = 1;
            break;
        }

        unsigned max_temp_mc = 0;
        unsigned samples = 0;
        int cancelled = 0;

        printf(
            "OPTIMIZER_VALIDATION: OPP_BEGIN frequency_khz=%u voltage_uv=%u\n",
            opp->frequency_khz,
            opp->optimized_uv);
        fflush(stdout);

        if (run_stress_window(
                seconds_per_opp,
                opp->frequency_khz,
                opp->optimized_uv,
                &hw_watchdog,
                1,
                &cancelled,
                &max_temp_mc,
                &samples) < 0) {
            failed = 1;
            break;
        }

        if (cancelled) {
            power_cancelled = 1;
            break;
        }

        if (run_integrity_check() < 0) {
            failed = 1;
            break;
        }

        if (max_temp_mc > global_max_temp)
            global_max_temp = max_temp_mc;

        tested_opps++;
        printf(
            "OPTIMIZER_VALIDATION: OPP_PASS frequency_khz=%u "
            "samples=%u max_temp_mc=%u progress=%u/%u\n",
            opp->frequency_khz,
            samples,
            max_temp_mc,
            tested_opps,
            total_opps);
        fflush(stdout);

        /*
         * Always return to the stock-preserved parking OPP between test
         * points. The next loop iteration may durably update validation
         * progress; never perform that SD write while still running at an
         * undervolted performance OPP.
         */
        if (flip_cpu_policy_set_limits(
                FLIP_OPTIMIZER_VALIDATION_PARK_KHZ,
                FLIP_OPTIMIZER_VALIDATION_PARK_KHZ) < 0 ||
            wait_for_frequency(
                FLIP_OPTIMIZER_VALIDATION_PARK_KHZ) < 0 ||
            flip_optimizer_hw_watchdog_ping(&hw_watchdog) < 0)
            emergency_reboot();
    }

    /*
     * Return to the stock-preserved parking OPP before touching the live OPP
     * table again. Restore stock voltages first; only then restore the user's
     * normal governor/frequency limits.
     */
    if (flip_cpu_policy_set_limits(
            FLIP_OPTIMIZER_VALIDATION_PARK_KHZ,
            FLIP_OPTIMIZER_VALIDATION_PARK_KHZ) < 0 ||
        wait_for_frequency(
            FLIP_OPTIMIZER_VALIDATION_PARK_KHZ) < 0)
        emergency_reboot();

    if (flip_optimizer_hw_watchdog_ping(&hw_watchdog) < 0)
        emergency_reboot();

    if (runtime_applied &&
        run_child_tool_with_watchdog(
            FLIP_OPTIMIZER_RUNTIME_TOOL,
            "restore",
            &hw_watchdog) < 0)
        emergency_reboot();
    runtime_applied = 0;

    if (original_valid && flip_cpu_policy_restore(&original) < 0)
        failed = 1;

    if (disarm_deadman(&deadman_pid) < 0)
        failed = 1;

    if (quiesced && flip_optimizer_quiesce_end(&quiesce) < 0)
        failed = 1;
    quiesced = 0;

    const char *status =
        power_cancelled ? "cancelled" :
        failed ? "failed" :
        "passed";

    if (write_validation_state_file(
            FLIP_OPTIMIZER_VALIDATION_RESULT,
            status,
            &result,
            tested_opps,
            seconds_per_opp,
            global_max_temp) < 0)
        emergency_reboot();

    (void)unlink(FLIP_OPTIMIZER_VALIDATION_ACTIVE);
    (void)unlink(FLIP_OPTIMIZER_VALIDATION_REQUEST);
    sync();

    printf(
        "OPTIMIZER_VALIDATION: COMPLETE status=%s tested=%u/%u max_temp_mc=%u reboot=1\n",
        status,
        tested_opps,
        total_opps,
        global_max_temp);
    fflush(stdout);

    /*
     * /dev/watchdog0 cannot be stopped once opened on this platform. Keep the
     * recovery route armed and reboot deliberately after all result/state data
     * is durable. The runtime profile has already been restored to stock.
     */
    emergency_reboot();

validation_abort_before_watchdog:
    if (tsadc_armed)
        (void)tsadc_recovery_set(0);
    if (tsadc_loaded)
        (void)tsadc_recovery_unload();
    if (panic_armed)
        (void)flip_optimizer_panic_policy_end(&panic_policy);
    if (runtime_applied) {
        (void)flip_cpu_policy_set_limits(
            FLIP_OPTIMIZER_VALIDATION_PARK_KHZ,
            FLIP_OPTIMIZER_VALIDATION_PARK_KHZ);
        (void)wait_for_frequency(
            FLIP_OPTIMIZER_VALIDATION_PARK_KHZ);
        (void)run_child_tool(FLIP_OPTIMIZER_RUNTIME_TOOL, "restore");
        runtime_applied = 0;
    }
    if (original_valid)
        (void)flip_cpu_policy_restore(&original);
    if (quiesced)
        (void)flip_optimizer_quiesce_end(&quiesce);
    (void)write_validation_state_file(
        FLIP_OPTIMIZER_VALIDATION_RESULT,
        "start-failed",
        &result,
        0,
        seconds_per_opp,
        0);
    (void)unlink(FLIP_OPTIMIZER_VALIDATION_ACTIVE);
    (void)unlink(FLIP_OPTIMIZER_VALIDATION_REQUEST);
    sync();
    return -1;
}

static void usage(const char *program)
{
    fprintf(
        stderr,
        "usage: %s integrity-only | table-only | quiesce-probe SECONDS | kernel-recovery-probe SECONDS | kernel-recovery-crash-test | watchdog-reboot-probe | watchdog-button-expiry-probe | live-once SECONDS | validate-profile SECONDS_PER_OPP\n",
        program);
}

int main(int argc, char **argv)
{
    if (argc == 2 &&
        strcmp(argv[1], "integrity-only") == 0) {
        if (run_integrity_check() < 0)
            return EXIT_FAILURE;

        printf("OPTIMIZER_CANDIDATE: INTEGRITY_ONLY_OK\n");
        return EXIT_SUCCESS;
    }

    if (argc == 2 &&
        strcmp(argv[1], "table-only") == 0)
        return run_table_only() == 0
            ? EXIT_SUCCESS
            : EXIT_FAILURE;

    if (argc == 3 &&
        strcmp(argv[1], "quiesce-probe") == 0) {
        unsigned seconds = 0;

        if (parse_unsigned_text(
                argv[2],
                &seconds) < 0) {
            usage(argv[0]);
            return EXIT_FAILURE;
        }

        return run_quiesce_probe(seconds) == 0
            ? EXIT_SUCCESS
            : EXIT_FAILURE;
    }

    if (argc == 2 &&
        strcmp(argv[1], "watchdog-button-expiry-probe") == 0)
        return run_watchdog_button_expiry_probe() == 0
            ? EXIT_SUCCESS
            : EXIT_FAILURE;

    if (argc == 2 &&
        strcmp(argv[1], "watchdog-reboot-probe") == 0)
        return run_watchdog_reboot_probe() == 0
            ? EXIT_SUCCESS
            : EXIT_FAILURE;

    if (argc == 2 &&
        strcmp(argv[1], "kernel-recovery-crash-test") == 0)
        return run_kernel_recovery_crash_test() == 0
            ? EXIT_SUCCESS
            : EXIT_FAILURE;

    if (argc == 3 &&
        strcmp(argv[1], "kernel-recovery-probe") == 0) {
        unsigned seconds = 0;

        if (parse_unsigned_text(
                argv[2],
                &seconds) < 0) {
            usage(argv[0]);
            return EXIT_FAILURE;
        }

        return run_kernel_recovery_probe(seconds) == 0
            ? EXIT_SUCCESS
            : EXIT_FAILURE;
    }

    if (argc == 3 &&
        strcmp(argv[1], "validate-profile") == 0) {
        unsigned seconds = 0;

        if (parse_unsigned_text(
                argv[2],
                &seconds) < 0) {
            usage(argv[0]);
            return EXIT_FAILURE;
        }

        return run_validate_profile(seconds) == 0
            ? EXIT_SUCCESS
            : EXIT_FAILURE;
    }

    if (argc == 3 &&
        strcmp(argv[1], "live-once") == 0) {
        unsigned seconds = 0;

        if (parse_unsigned_text(
                argv[2],
                &seconds) < 0) {
            usage(argv[0]);
            return EXIT_FAILURE;
        }

        return run_live_once(seconds) == 0
            ? EXIT_SUCCESS
            : EXIT_FAILURE;
    }

    usage(argv[0]);
    return EXIT_FAILURE;
}
