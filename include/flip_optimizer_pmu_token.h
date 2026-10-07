#ifndef FLIP_OPTIMIZER_PMU_TOKEN_H
#define FLIP_OPTIMIZER_PMU_TOKEN_H

#include <stddef.h>
#include <stdint.h>

#define FLIP_OPTIMIZER_PMU_TOKEN_PHYS 0xfdc20204ULL
#define FLIP_OPTIMIZER_PMU_TOKEN_TRIAL_MAX 127

int flip_optimizer_pmu_token_encode_trial(
    const char *trial_id,
    uint32_t *token);

int flip_optimizer_pmu_token_decode_trial(
    uint32_t token,
    char *trial_id,
    size_t trial_id_size);

int flip_optimizer_pmu_token_write_trial(
    const char *trial_id);

int flip_optimizer_pmu_token_read_trial(
    char *trial_id,
    size_t trial_id_size);

int flip_optimizer_pmu_token_clear(void);

#endif
