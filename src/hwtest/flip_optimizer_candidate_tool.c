#include "flip_cpu_policy.h"
#include "flip_optimizer_calibration.h"
#include "flip_optimizer_calibration_session.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/reboot.h>
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
#define FLIP_OPTIMIZER_MODULE_CONTROL \
    "/sys/module/flip_opp_voltage/parameters/control"

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

static void emergency_reboot(void)
{
    sync();
    reboot(RB_AUTOBOOT);
    _exit(EXIT_FAILURE);
}

static int ensure_candidate_ready(
    flip_optimizer_calibration_t *calibration)
{
    if (!calibration)
        return -1;

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

static void usage(const char *program)
{
    fprintf(
        stderr,
        "usage: %s table-only\n",
        program);
}

int main(int argc, char **argv)
{
    if (argc == 2 &&
        strcmp(argv[1], "table-only") == 0)
        return run_table_only() == 0
            ? EXIT_SUCCESS
            : EXIT_FAILURE;

    usage(argv[0]);
    return EXIT_FAILURE;
}
