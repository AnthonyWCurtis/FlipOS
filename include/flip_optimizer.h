#ifndef FLIP_OPTIMIZER_H
#define FLIP_OPTIMIZER_H

#include <stddef.h>

#define FLIP_OPTIMIZER_VERSION 1
#define FLIP_OPTIMIZER_MAX_OPPS 16
#define FLIP_OPTIMIZER_DEVICE_ID_MAX 96
#define FLIP_OPTIMIZER_MODEL_MAX 64

#ifndef FLIP_OPTIMIZER_ROOT
#define FLIP_OPTIMIZER_ROOT "/mnt/SDCARD/flipos-dev/state/optimizer"
#endif

#ifndef FLIP_OPTIMIZER_RESULT_FILE
#define FLIP_OPTIMIZER_RESULT_FILE FLIP_OPTIMIZER_ROOT "/cpu-result"
#endif

#ifndef FLIP_OPTIMIZER_REPORT_FILE
#define FLIP_OPTIMIZER_REPORT_FILE FLIP_OPTIMIZER_ROOT "/cpu-report"
#endif

typedef struct {
    unsigned frequency_khz;
    unsigned stock_uv;
    unsigned optimized_uv;
} flip_optimizer_opp_t;

typedef struct {
    int version;
    char device_id[FLIP_OPTIMIZER_DEVICE_ID_MAX];
    char model[FLIP_OPTIMIZER_MODEL_MAX];
    unsigned calibrated_epoch;
    unsigned guardband_uv;
    size_t opp_count;
    flip_optimizer_opp_t opps[FLIP_OPTIMIZER_MAX_OPPS];

    double stock_power_mw;
    double optimized_power_mw;
    unsigned baseline_runtime_minutes;
    int power_measurement_valid;
} flip_optimizer_result_t;

typedef struct {
    double top_opp_voltage_reduction_mv;
    double top_opp_cpu_dynamic_reduction_percent;
    double whole_device_power_reduction_percent;
    double estimated_runtime_minutes;
    double estimated_extra_runtime_minutes;
    int measured_power_valid;
} flip_optimizer_summary_t;

int flip_optimizer_validate(const flip_optimizer_result_t *result);
int flip_optimizer_load(flip_optimizer_result_t *result);
int flip_optimizer_save(const flip_optimizer_result_t *result);
int flip_optimizer_summarize(
    const flip_optimizer_result_t *result,
    flip_optimizer_summary_t *summary);

#endif
