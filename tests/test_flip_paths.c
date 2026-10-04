#include "flip_paths.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    assert(strcmp(
        flip_paths_storage_root(),
        FLIP_STORAGE_ROOT) == 0);

    assert(strcmp(
        flip_paths_roms_root(),
        FLIP_ROMS_ROOT) == 0);

    assert(strcmp(
        flip_paths_bin_root(),
        FLIP_BIN_ROOT) == 0);

    assert(strcmp(
        flip_paths_state_data_root(),
        FLIP_STATE_DATA_ROOT) == 0);

    assert(strcmp(
        flip_paths_cache_root(),
        FLIP_CACHE_ROOT) == 0);

    assert(strcmp(
        flip_paths_system_root(),
        FLIP_SYSTEM_ROOT) == 0);

    assert(strcmp(
        flip_paths_save_root(),
        FLIP_SAVE_ROOT) == 0);

    assert(strcmp(
        flip_paths_state_root(),
        FLIP_STATE_ROOT) == 0);

    assert(strcmp(
        flip_paths_recovery_root(),
        FLIP_SESSION_RECOVERY_ROOT) == 0);

    assert(strcmp(
        flip_paths_settings_root(),
        FLIP_SETTINGS_ROOT) == 0);

    assert(strcmp(
        flip_paths_session_root(),
        FLIP_SESSION_ROOT) == 0);

    assert(strcmp(
        flip_paths_recent_root(),
        FLIP_RECENT_ROOT) == 0);

    assert(strcmp(
        FLIP_SETTINGS_FILE,
        FLIP_SETTINGS_ROOT "/global") == 0);

    assert(strcmp(
        FLIP_SESSION_FILE,
        FLIP_SESSION_ROOT "/current") == 0);

    assert(strcmp(
        FLIP_SESSION_REGISTRY_FILE,
        FLIP_SESSION_REGISTRY_ROOT "/registry") == 0);

    assert(strcmp(
        FLIP_RECENT_FILE,
        FLIP_RECENT_ROOT "/history") == 0);

    puts("flip_paths tests passed");
    return 0;
}
