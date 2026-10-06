#include "flip_optimizer_calibration.h"
#include "flip_optimizer_calibration_session.h"
#include "flip_optimizer_guard.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CPUINFO_PATH "/proc/cpuinfo"
#define MODEL_PATH "/proc/device-tree/model"
#define REGULATOR_ROOT "/sys/class/regulator"
#define CPU_OPP_ROOT "/sys/kernel/debug/opp/cpu0"

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
        read_regulator_floor(floor_uv) < 0 ||
        read_cpu_opps(
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
        if (opp->stock_uv >=
            opp->floor_uv +
                calibration->voltage_step_uv) {
            first_candidate =
                opp->stock_uv -
                calibration->voltage_step_uv;
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

    return flip_optimizer_calibration_init(
        calibration,
        device_id,
        model,
        opps,
        opp_count,
        FLIP_OPTIMIZER_CALIBRATION_DEFAULT_STEP_UV,
        FLIP_OPTIMIZER_CALIBRATION_DEFAULT_GUARDBAND_UV);
}

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

static void usage(const char *program)
{
    fprintf(
        stderr,
        "usage: %s probe | init | status | prepare | ready | "
        "reconcile | confirm-stable | clear-stock-only\n",
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

    if (strcmp(argv[1], "init") == 0) {
        flip_optimizer_calibration_t calibration;

        if (make_live_calibration(&calibration) < 0 ||
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
