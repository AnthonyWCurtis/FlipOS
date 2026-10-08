#include "flip_cpu_policy.h"
#include "flip_optimizer.h"
#include "flip_optimizer_guard.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#define RUNTIME_MODULE_NAME "flip_opp_runtime_profile"
#define RUNTIME_MODULE_PATH "/mnt/SDCARD/flipos-dev/lib/flip_opp_runtime_profile.ko"
#define RUNTIME_CONTROL "/sys/module/flip_opp_runtime_profile/parameters/control"
#define CPU_OPP_ROOT "/sys/kernel/debug/opp/cpu0"
#define CPUINFO_PATH "/proc/cpuinfo"
#define SESSION_FILE "/mnt/SDCARD/flipos-dev/state/optimizer/auto-session"
#define UI_RETURN_FILE "/mnt/SDCARD/flipos-dev/state/optimizer/ui-return"
#define PARK_KHZ 408000U

typedef struct {
    unsigned target_uv;
    unsigned min_uv;
    unsigned max_uv;
} opp_triplet_t;

static int read_text(const char *path, char *buffer, size_t size)
{
    FILE *f;
    size_t n;

    if (!path || !buffer || size < 2)
        return -1;

    f = fopen(path, "r");
    if (!f)
        return -1;

    n = fread(buffer, 1, size - 1, f);
    if (ferror(f)) {
        fclose(f);
        return -1;
    }

    fclose(f);
    if (!n)
        return -1;

    buffer[n] = '\0';
    while (n &&
           (buffer[n - 1] == '\n' ||
            buffer[n - 1] == '\r' ||
            buffer[n - 1] == ' ' ||
            buffer[n - 1] == '\t'))
        buffer[--n] = '\0';

    return n ? 0 : -1;
}

static int read_unsigned(const char *path, unsigned *value)
{
    char text[64];
    char *end = NULL;
    unsigned long parsed;

    if (!value || read_text(path, text, sizeof(text)) < 0)
        return -1;

    errno = 0;
    parsed = strtoul(text, &end, 10);
    if (errno || end == text || *end != '\0' || parsed > 0xffffffffUL)
        return -1;

    *value = (unsigned)parsed;
    return 0;
}

static int write_text(const char *path, const char *value)
{
    FILE *f;
    int failed;

    if (!path || !value || !value[0])
        return -1;

    f = fopen(path, "w");
    if (!f)
        return -1;

    failed = fprintf(f, "%s\n", value) < 0;
    if (!failed && fflush(f) != 0)
        failed = 1;
    if (fclose(f) != 0)
        failed = 1;

    return failed ? -1 : 0;
}

static int read_device_id(char *device_id, size_t size)
{
    FILE *f = fopen(CPUINFO_PATH, "r");
    char line[256];

    if (!f || !device_id || size < 2)
        return -1;

    while (fgets(line, sizeof(line), f)) {
        char *colon;
        char *value;
        char *end;

        if (strncmp(line, "Serial", 6) != 0)
            continue;

        colon = strchr(line, ':');
        if (!colon)
            continue;

        value = colon + 1;
        while (*value == ' ' || *value == '\t')
            ++value;

        end = strpbrk(value, "\r\n");
        if (end)
            *end = '\0';

        if (!value[0] || strlen(value) >= size)
            break;

        strcpy(device_id, value);
        fclose(f);
        return 0;
    }

    fclose(f);
    return -1;
}

static int make_opp_path(
    char *path,
    size_t size,
    unsigned freq_khz,
    const char *name)
{
    unsigned long long hz = (unsigned long long)freq_khz * 1000ULL;
    int n = snprintf(
        path,
        size,
        "%s/opp:%llu/supply-0/%s",
        CPU_OPP_ROOT,
        hz,
        name);

    return n >= 0 && (size_t)n < size ? 0 : -1;
}

static int read_opp_triplet(unsigned freq_khz, opp_triplet_t *triplet)
{
    char path[512];

    if (!triplet)
        return -1;

    if (make_opp_path(path, sizeof(path), freq_khz, "u_volt_target") < 0 ||
        read_unsigned(path, &triplet->target_uv) < 0 ||
        make_opp_path(path, sizeof(path), freq_khz, "u_volt_min") < 0 ||
        read_unsigned(path, &triplet->min_uv) < 0 ||
        make_opp_path(path, sizeof(path), freq_khz, "u_volt_max") < 0 ||
        read_unsigned(path, &triplet->max_uv) < 0)
        return -1;

    return 0;
}

static int module_loaded(void)
{
    struct stat st;
    return stat("/sys/module/" RUNTIME_MODULE_NAME, &st) == 0;
}

static int load_module(void)
{
    int fd;
    long rc;
    int saved_errno;

    if (module_loaded())
        return -1;

    fd = open(RUNTIME_MODULE_PATH, O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return -1;

    rc = syscall(SYS_finit_module, fd, "", 0);
    saved_errno = errno;
    close(fd);
    errno = saved_errno;
    return rc == 0 ? 0 : -1;
}

static int unload_module(void)
{
    long rc;

    if (!module_loaded())
        return 0;

    rc = syscall(SYS_delete_module, RUNTIME_MODULE_NAME, O_NONBLOCK);
    return rc == 0 ? 0 : -1;
}

static int park_cpu(
    flip_cpu_policy_snapshot_t *snapshot)
{
    unsigned current = 0;

    if (!snapshot ||
        flip_cpu_policy_snapshot(snapshot) < 0 ||
        flip_cpu_policy_set_limits(PARK_KHZ, PARK_KHZ) < 0)
        return -1;

    for (int i = 0; i < 50; ++i) {
        if (flip_cpu_policy_get_current(&current) == 0 &&
            current == PARK_KHZ)
            return 0;

        struct timespec delay = {
            .tv_sec = 0,
            .tv_nsec = 20 * 1000 * 1000,
        };
        nanosleep(&delay, NULL);
    }

    (void)flip_cpu_policy_restore(snapshot);
    return -1;
}

static int apply_one(
    const flip_optimizer_opp_t *opp,
    const opp_triplet_t *stock)
{
    char command[256];
    int n;

    if (!opp || !stock)
        return -1;

    n = snprintf(
        command,
        sizeof(command),
        "apply,%u,%u,%u,%u,%u",
        opp->frequency_khz,
        opp->optimized_uv,
        stock->target_uv,
        stock->min_uv,
        stock->max_uv);

    if (n < 0 || (size_t)n >= sizeof(command))
        return -1;

    return write_text(RUNTIME_CONTROL, command);
}

static int restore_loaded_profile(void)
{
    flip_cpu_policy_snapshot_t snapshot;
    int parked = 0;
    int rc = -1;

    if (!module_loaded())
        return 0;

    if (park_cpu(&snapshot) < 0)
        return -1;
    parked = 1;

    if (write_text(RUNTIME_CONTROL, "restore-all") < 0)
        goto out;

    if (unload_module() < 0)
        goto out;

    rc = 0;

out:
    if (parked && flip_cpu_policy_restore(&snapshot) < 0)
        rc = -1;
    return rc;
}

static int runtime_preconditions(
    flip_optimizer_result_t *result)
{
    flip_optimizer_guard_status_t guard;
    char device_id[FLIP_OPTIMIZER_DEVICE_ID_MAX];

    if (!result ||
        access(SESSION_FILE, F_OK) == 0 ||
        access(UI_RETURN_FILE, F_OK) == 0 ||
        flip_optimizer_guard_status(&guard) < 0 ||
        guard.active ||
        guard.stock_only ||
        flip_optimizer_load(result) < 0 ||
        read_device_id(device_id, sizeof(device_id)) < 0 ||
        strcmp(device_id, result->device_id) != 0)
        return -1;

    return 0;
}

static int apply_profile(void)
{
    flip_optimizer_result_t result;
    flip_cpu_policy_snapshot_t snapshot;
    int parked = 0;
    int loaded = 0;
    unsigned applied = 0;
    int rc = -1;

    if (module_loaded())
        return -1;

    if (runtime_preconditions(&result) < 0)
        return -1;

    if (park_cpu(&snapshot) < 0)
        return -1;
    parked = 1;

    if (load_module() < 0)
        goto rollback;
    loaded = 1;

    for (size_t i = 0; i < result.opp_count; ++i) {
        const flip_optimizer_opp_t *opp = &result.opps[i];
        opp_triplet_t stock;

        if (opp->optimized_uv == opp->stock_uv)
            continue;

        if (read_opp_triplet(opp->frequency_khz, &stock) < 0 ||
            stock.target_uv != opp->stock_uv ||
            stock.min_uv > stock.target_uv ||
            stock.target_uv > stock.max_uv ||
            apply_one(opp, &stock) < 0)
            goto rollback;

        applied++;
    }

    if (!applied)
        goto rollback;

    rc = 0;
    goto out;

rollback:
    if (loaded) {
        (void)write_text(RUNTIME_CONTROL, "restore-all");
        (void)unload_module();
    }

out:
    if (parked && flip_cpu_policy_restore(&snapshot) < 0)
        rc = -1;

    if (rc == 0) {
        printf("runtime_profile_applied=1\n");
        printf("optimized_opps=%u\n", applied);
    }

    return rc;
}

static int print_status(void)
{
    char buffer[2048];

    if (!module_loaded()) {
        printf("runtime_profile_active=0\n");
        return 0;
    }

    if (read_text(RUNTIME_CONTROL, buffer, sizeof(buffer)) < 0)
        return -1;

    printf("runtime_profile_active=1\n");
    printf("%s\n", buffer);
    return 0;
}

static void usage(const char *program)
{
    fprintf(stderr, "usage: %s apply | restore | status\n", program);
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "apply") == 0)
        return apply_profile() == 0 ? EXIT_SUCCESS : EXIT_FAILURE;

    if (strcmp(argv[1], "restore") == 0)
        return restore_loaded_profile() == 0 ? EXIT_SUCCESS : EXIT_FAILURE;

    if (strcmp(argv[1], "status") == 0)
        return print_status() == 0 ? EXIT_SUCCESS : EXIT_FAILURE;

    usage(argv[0]);
    return EXIT_FAILURE;
}
