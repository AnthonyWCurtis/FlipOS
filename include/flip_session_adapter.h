#ifndef FLIP_SESSION_ADAPTER_H
#define FLIP_SESSION_ADAPTER_H

#include "flip_session.h"

#include <stddef.h>

typedef struct {
    const char *system;

    flip_session_action_t (*map_exit_code)(
        int exit_code);

    int (*resume_available)(
        const char *rom_path);

    int (*supports_save_safety)(void);

    int (*build_start_normal_argv)(
        const char *runner,
        const char *rom_path,
        char **argv,
        size_t argv_count);

    int (*build_save_safety_argv)(
        const char *runner,
        const char *rom_path,
        const char *action,
        char **argv,
        size_t argv_count);
} flip_session_adapter_t;

const flip_session_adapter_t *
flip_session_adapter_for_system(
    const char *system);

#endif
