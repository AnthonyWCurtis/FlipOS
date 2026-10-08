#ifndef FLIP_OPTIMIZER_CALIBRATION_H
#define FLIP_OPTIMIZER_CALIBRATION_H

#include "flip_optimizer.h"

#include <stddef.h>

#define FLIP_OPTIMIZER_CALIBRATION_VERSION 1
#define FLIP_OPTIMIZER_CALIBRATION_MAX_OPPS FLIP_OPTIMIZER_MAX_OPPS
#define FLIP_OPTIMIZER_CALIBRATION_TRIAL_ID_MAX 64
#define FLIP_OPTIMIZER_CALIBRATION_DEFAULT_STEP_UV 12500U
#define FLIP_OPTIMIZER_CALIBRATION_DEFAULT_GUARDBAND_UV 25000U
#define FLIP_OPTIMIZER_CALIBRATION_RK3566_SAFE_MIN_UV 812500U
#define FLIP_OPTIMIZER_CALIBRATION_RK3566_MIN_TUNABLE_KHZ 1104000U

#ifndef FLIP_OPTIMIZER_CALIBRATION_ROOT
#define FLIP_OPTIMIZER_CALIBRATION_ROOT "/mnt/SDCARD/flipos-dev/state/optimizer"
#endif

#ifndef FLIP_OPTIMIZER_CALIBRATION_FILE
#define FLIP_OPTIMIZER_CALIBRATION_FILE \
    FLIP_OPTIMIZER_CALIBRATION_ROOT "/cpu-calibration"
#endif

typedef struct {
    unsigned frequency_khz;
    unsigned stock_uv;
    unsigned floor_uv;
} flip_optimizer_calibration_stock_opp_t;

typedef struct {
    unsigned frequency_khz;
    unsigned stock_uv;
    unsigned floor_uv;
    unsigned lowest_stable_uv;
    unsigned highest_unstable_uv;
    unsigned raw_final_uv;
    unsigned tests_run;
    int complete;
} flip_optimizer_calibration_opp_t;

typedef struct {
    int version;
    char device_id[FLIP_OPTIMIZER_DEVICE_ID_MAX];
    char model[FLIP_OPTIMIZER_MODEL_MAX];
    unsigned voltage_step_uv;
    unsigned guardband_uv;
    size_t opp_count;

    int pending;
    size_t pending_index;
    unsigned pending_uv;
    char pending_trial_id[FLIP_OPTIMIZER_CALIBRATION_TRIAL_ID_MAX + 1];

    flip_optimizer_calibration_opp_t
        opps[FLIP_OPTIMIZER_CALIBRATION_MAX_OPPS];
} flip_optimizer_calibration_t;

int flip_optimizer_calibration_init(
    flip_optimizer_calibration_t *calibration,
    const char *device_id,
    const char *model,
    const flip_optimizer_calibration_stock_opp_t *opps,
    size_t opp_count,
    unsigned voltage_step_uv,
    unsigned guardband_uv);

int flip_optimizer_calibration_validate(
    const flip_optimizer_calibration_t *calibration);

int flip_optimizer_calibration_save(
    const flip_optimizer_calibration_t *calibration);

int flip_optimizer_calibration_load(
    flip_optimizer_calibration_t *calibration);

int flip_optimizer_calibration_prepare_next(
    flip_optimizer_calibration_t *calibration,
    size_t *opp_index,
    unsigned *candidate_uv);

int flip_optimizer_calibration_record_pending(
    flip_optimizer_calibration_t *calibration,
    int stable);

int flip_optimizer_calibration_record_rollback(
    flip_optimizer_calibration_t *calibration,
    const char *trial_id);

int flip_optimizer_calibration_finalize_after_hard_rollback(
    flip_optimizer_calibration_t *calibration,
    size_t opp_index);

int flip_optimizer_calibration_cancel_pending(
    flip_optimizer_calibration_t *calibration);

int flip_optimizer_calibration_preserve_stock_below(
    flip_optimizer_calibration_t *calibration,
    unsigned min_frequency_khz);

int flip_optimizer_calibration_is_complete(
    const flip_optimizer_calibration_t *calibration);

int flip_optimizer_calibration_build_result(
    const flip_optimizer_calibration_t *calibration,
    unsigned calibrated_epoch,
    flip_optimizer_result_t *result);

#endif
