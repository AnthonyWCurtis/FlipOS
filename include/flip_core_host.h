#ifndef FLIP_CORE_HOST_H
#define FLIP_CORE_HOST_H

#include "flip_core.h"

typedef int (*flip_core_host_state_fn)(
    flip_core_adapter_t *core,
    void *userdata);

typedef int (*flip_core_host_persistence_complete_fn)(
    void *userdata);

typedef struct {
    flip_core_adapter_t *core;
    const char *rom_path;

    double fallback_fps;

    flip_core_host_state_fn load_save;
    flip_core_host_state_fn commit_save;
    void *save_userdata;

    flip_core_host_state_fn save_state;
    flip_core_host_state_fn restore_state;
    void *state_userdata;

    /*
     * Optional reconciliation step after a successful savestate restore.
     * Used for state that must follow current wall-clock time (for example,
     * cartridge RTC memory) rather than the historical snapshot in the state.
     */
    flip_core_host_state_fn post_restore;
    void *post_restore_userdata;

    flip_core_host_persistence_complete_fn persistence_complete;
    void *persistence_userdata;
} flip_core_host_config_t;

/*
 * Run one emulator process through the shared FlipOS host path.
 *
 * Returns 0 when the host ran and shut down cleanly. The
 * lifecycle-selected process exit code is written to exit_code.
 * Startup/runtime failures return -1 after best-effort cleanup.
 */
int flip_core_host_run(
    const flip_core_host_config_t *config,
    int *exit_code);

#endif
