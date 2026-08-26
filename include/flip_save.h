#ifndef FLIP_SAVE_H
#define FLIP_SAVE_H

#include "flip_core.h"

#include <stddef.h>

#ifndef FLIP_SAVE_ROOT
#define FLIP_SAVE_ROOT "/mnt/SDCARD/flipos-dev/saves"
#endif

typedef struct {
    const char *system_id;
    const char *rom_path;
} flip_save_context_t;

typedef enum {
    FLIP_SAVE_LOAD_NO_MEMORY = 0,
    FLIP_SAVE_LOAD_NO_SAVE = 1,
    FLIP_SAVE_LOAD_CURRENT = 2,
    FLIP_SAVE_LOAD_PREVIOUS = 3,
    FLIP_SAVE_LOAD_UNUSABLE = 4,
    FLIP_SAVE_LOAD_ERROR = 5
} flip_save_load_result_t;

void flip_save_context_init(
    flip_save_context_t *context,
    const char *system_id,
    const char *rom_path);

flip_save_load_result_t flip_save_load(
    flip_core_adapter_t *core,
    const flip_save_context_t *context);

int flip_save_commit(
    flip_core_adapter_t *core,
    const flip_save_context_t *context);

#endif
