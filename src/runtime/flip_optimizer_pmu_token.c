#include "flip_optimizer_pmu_token.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

#ifndef FLIP_OPTIMIZER_PMU_TOKEN_TEST_FILE
#include <sys/mman.h>
#include <unistd.h>
#endif

#define TOKEN_MAGIC 0xf1U
#define TOKEN_VERSION_LEGACY 1U
#define TOKEN_VERSION_CURRENT 2U

static const unsigned frequencies_khz[] = {
    408000U,
    600000U,
    816000U,
    1104000U,
    1416000U,
    1608000U,
    1800000U,
    1992000U,
};

static unsigned token_checksum(uint32_t body)
{
    uint32_t value = body >> 6;
    value ^= value >> 6;
    value ^= value >> 12;
    value ^= value >> 18;
    return value & 0x3fU;
}

int flip_optimizer_pmu_token_encode_trial_phase(
    const char *trial_id,
    flip_optimizer_pmu_phase_t phase,
    uint32_t *token)
{
    if (!trial_id || !token ||
        (phase != FLIP_OPTIMIZER_PMU_PHASE_PREPARED &&
         phase != FLIP_OPTIMIZER_PMU_PHASE_ENTERING))
        return -1;

    unsigned frequency_khz = 0;
    unsigned candidate_uv = 0;
    unsigned trial_number = 0;
    char trailing = 0;

    if (sscanf(
            trial_id,
            "cpu-%u-%u-%u%c",
            &frequency_khz,
            &candidate_uv,
            &trial_number,
            &trailing) != 3)
        return 0;

    unsigned frequency_index =
        (unsigned)(sizeof(frequencies_khz) /
                   sizeof(frequencies_khz[0]));

    for (unsigned i = 0;
         i < sizeof(frequencies_khz) / sizeof(frequencies_khz[0]);
         ++i) {
        if (frequencies_khz[i] == frequency_khz) {
            frequency_index = i;
            break;
        }
    }

    if (frequency_index >=
            sizeof(frequencies_khz) / sizeof(frequencies_khz[0]) ||
        candidate_uv % 12500U != 0 ||
        trial_number == 0 ||
        trial_number > 31U)
        return 0;

    unsigned uv_code = candidate_uv / 12500U;
    if (uv_code > 127U)
        return 0;

    unsigned phase_bit =
        phase == FLIP_OPTIMIZER_PMU_PHASE_ENTERING
            ? 1U
            : 0U;

    uint32_t body =
        (TOKEN_MAGIC << 24) |
        (TOKEN_VERSION_CURRENT << 22) |
        ((frequency_index & 0x7U) << 19) |
        ((uv_code & 0x7fU) << 12) |
        ((phase_bit & 0x1U) << 11) |
        ((trial_number & 0x1fU) << 6);

    *token = body | token_checksum(body);
    return 1;
}

int flip_optimizer_pmu_token_encode_trial(
    const char *trial_id,
    uint32_t *token)
{
    return flip_optimizer_pmu_token_encode_trial_phase(
        trial_id,
        FLIP_OPTIMIZER_PMU_PHASE_PREPARED,
        token);
}

int flip_optimizer_pmu_token_decode_trial_phase(
    uint32_t token,
    char *trial_id,
    size_t trial_id_size,
    flip_optimizer_pmu_phase_t *phase)
{
    if (!trial_id || trial_id_size == 0 || !phase)
        return -1;

    trial_id[0] = '\0';
    *phase = FLIP_OPTIMIZER_PMU_PHASE_PREPARED;

    if (token == 0)
        return 0;

    if ((token >> 24) != TOKEN_MAGIC)
        return -2;

    unsigned version = (token >> 22) & 0x3U;
    if (version != TOKEN_VERSION_LEGACY &&
        version != TOKEN_VERSION_CURRENT)
        return -2;

    uint32_t body = token & ~0x3fU;
    if ((token & 0x3fU) != token_checksum(body))
        return -2;

    unsigned frequency_index = (token >> 19) & 0x7U;
    unsigned uv_code = (token >> 12) & 0x7fU;
    unsigned trial_number = 0;

    if (version == TOKEN_VERSION_LEGACY) {
        trial_number = (token >> 6) & 0x3fU;
        *phase = FLIP_OPTIMIZER_PMU_PHASE_ENTERING;
    } else {
        trial_number = (token >> 6) & 0x1fU;
        *phase =
            ((token >> 11) & 0x1U)
                ? FLIP_OPTIMIZER_PMU_PHASE_ENTERING
                : FLIP_OPTIMIZER_PMU_PHASE_PREPARED;
    }

    if (frequency_index >=
            sizeof(frequencies_khz) / sizeof(frequencies_khz[0]) ||
        uv_code == 0 ||
        trial_number == 0)
        return -2;

    unsigned candidate_uv = uv_code * 12500U;

    int n = snprintf(
        trial_id,
        trial_id_size,
        "cpu-%u-%u-%u",
        frequencies_khz[frequency_index],
        candidate_uv,
        trial_number);

    if (n < 0 || (size_t)n >= trial_id_size) {
        trial_id[0] = '\0';
        return -1;
    }

    return 1;
}

int flip_optimizer_pmu_token_decode_trial(
    uint32_t token,
    char *trial_id,
    size_t trial_id_size)
{
    flip_optimizer_pmu_phase_t phase;
    return flip_optimizer_pmu_token_decode_trial_phase(
        token,
        trial_id,
        trial_id_size,
        &phase);
}

#ifdef FLIP_OPTIMIZER_PMU_TOKEN_TEST_FILE

static int backend_read(uint32_t *value)
{
    if (!value)
        return -1;

    FILE *file =
        fopen(FLIP_OPTIMIZER_PMU_TOKEN_TEST_FILE, "r");

    if (!file) {
        if (errno == ENOENT) {
            *value = 0;
            return 0;
        }
        return -1;
    }

    unsigned parsed = 0;
    int rc = fscanf(file, "%x", &parsed);
    int close_rc = fclose(file);

    if (rc != 1 || close_rc != 0)
        return -1;

    *value = (uint32_t)parsed;
    return 0;
}

static int backend_write(uint32_t value)
{
    if (value == 0) {
        if (remove(FLIP_OPTIMIZER_PMU_TOKEN_TEST_FILE) == 0 ||
            errno == ENOENT)
            return 0;
        return -1;
    }

    FILE *file =
        fopen(FLIP_OPTIMIZER_PMU_TOKEN_TEST_FILE, "w");
    if (!file)
        return -1;

    int failed =
        fprintf(file, "%08x\n", value) < 0 ||
        fflush(file) != 0 ||
        fclose(file) != 0;

    return failed ? -1 : 0;
}

#else

static int map_register(
    int *fd_out,
    void **mapping_out,
    volatile uint32_t **register_out)
{
    if (!fd_out || !mapping_out || !register_out)
        return -1;

    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0)
        return -1;

    uintptr_t physical =
        (uintptr_t)FLIP_OPTIMIZER_PMU_TOKEN_PHYS;
    uintptr_t base =
        physical & ~((uintptr_t)page_size - 1U);
    uintptr_t offset = physical - base;

    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0)
        return -1;

    void *mapping = mmap(
        NULL,
        (size_t)page_size,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd,
        (off_t)base);

    if (mapping == MAP_FAILED) {
        close(fd);
        return -1;
    }

    *fd_out = fd;
    *mapping_out = mapping;
    *register_out =
        (volatile uint32_t *)((unsigned char *)mapping + offset);
    return 0;
}

static int backend_read(uint32_t *value)
{
    if (!value)
        return -1;

    int fd = -1;
    void *mapping = NULL;
    volatile uint32_t *reg = NULL;

    if (map_register(&fd, &mapping, &reg) < 0)
        return -1;

    *value = *reg;
    __sync_synchronize();

    long page_size = sysconf(_SC_PAGESIZE);
    int unmap_rc =
        munmap(mapping, (size_t)page_size);
    int close_rc = close(fd);

    return unmap_rc == 0 && close_rc == 0
        ? 0
        : -1;
}

static int backend_write(uint32_t value)
{
    int fd = -1;
    void *mapping = NULL;
    volatile uint32_t *reg = NULL;

    if (map_register(&fd, &mapping, &reg) < 0)
        return -1;

    *reg = value;
    __sync_synchronize();
    uint32_t readback = *reg;

    long page_size = sysconf(_SC_PAGESIZE);
    int unmap_rc =
        munmap(mapping, (size_t)page_size);
    int close_rc = close(fd);

    if (unmap_rc != 0 ||
        close_rc != 0 ||
        readback != value)
        return -1;

    return 0;
}

#endif

int flip_optimizer_pmu_token_write_trial_phase(
    const char *trial_id,
    flip_optimizer_pmu_phase_t phase)
{
    uint32_t token = 0;
    int rc = flip_optimizer_pmu_token_encode_trial_phase(
        trial_id,
        phase,
        &token);

    if (rc <= 0)
        return rc;

    return backend_write(token) == 0 ? 1 : -1;
}

int flip_optimizer_pmu_token_read_trial_phase(
    char *trial_id,
    size_t trial_id_size,
    flip_optimizer_pmu_phase_t *phase)
{
    uint32_t token = 0;

    if (backend_read(&token) < 0)
        return -1;

    return flip_optimizer_pmu_token_decode_trial_phase(
        token,
        trial_id,
        trial_id_size,
        phase);
}

int flip_optimizer_pmu_token_write_trial(
    const char *trial_id)
{
    return flip_optimizer_pmu_token_write_trial_phase(
        trial_id,
        FLIP_OPTIMIZER_PMU_PHASE_PREPARED);
}

int flip_optimizer_pmu_token_read_trial(
    char *trial_id,
    size_t trial_id_size)
{
    flip_optimizer_pmu_phase_t phase;
    return flip_optimizer_pmu_token_read_trial_phase(
        trial_id,
        trial_id_size,
        &phase);
}

int flip_optimizer_pmu_token_clear(void)
{
    return backend_write(0);
}
