#ifndef FLIP_SESSION_ADAPTER_H
#define FLIP_SESSION_ADAPTER_H

#include "flip_session.h"

typedef struct {
    const char *system;

    flip_session_action_t (*map_exit_code)(
        int exit_code);

    int (*resume_available)(
        const char *rom_path);
} flip_session_adapter_t;

const flip_session_adapter_t *
flip_session_adapter_for_system(
    const char *system);

#endif
