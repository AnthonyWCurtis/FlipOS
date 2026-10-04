#ifndef FLIP_EMULATOR_RUNNER_H
#define FLIP_EMULATOR_RUNNER_H

#include "flip_core.h"

typedef struct {
    const char *storage_id;
    const char *log_name;
    double fallback_fps;
    flip_core_adapter_t *core;
} flip_emulator_runner_config_t;

int flip_emulator_run(
    const flip_emulator_runner_config_t *config,
    int argc,
    char **argv);

#endif
