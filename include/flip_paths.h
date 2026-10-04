#ifndef FLIP_PATHS_H
#define FLIP_PATHS_H

/*
 * Canonical FlipOS storage layout.
 *
 * Runtime modules should consume these paths instead of embedding
 * SD-card locations themselves. Individual roots remain overrideable
 * at compile time so host tests can use temporary directories.
 */

#ifndef FLIP_STORAGE_ROOT
#define FLIP_STORAGE_ROOT "/mnt/SDCARD/flipos-dev"
#endif

#ifndef FLIP_ROMS_ROOT
#define FLIP_ROMS_ROOT "/mnt/SDCARD/Roms"
#endif

#ifndef FLIP_BIN_ROOT
#define FLIP_BIN_ROOT FLIP_STORAGE_ROOT "/bin"
#endif

#ifndef FLIP_STATE_DATA_ROOT
#define FLIP_STATE_DATA_ROOT FLIP_STORAGE_ROOT "/state"
#endif

#ifndef FLIP_CACHE_ROOT
#define FLIP_CACHE_ROOT FLIP_STORAGE_ROOT "/cache"
#endif

#ifndef FLIP_SYSTEM_ROOT
#define FLIP_SYSTEM_ROOT FLIP_STORAGE_ROOT "/system"
#endif

#ifndef FLIP_SAVE_ROOT
#define FLIP_SAVE_ROOT FLIP_STORAGE_ROOT "/saves"
#endif

#ifndef FLIP_STATE_ROOT
#define FLIP_STATE_ROOT FLIP_STORAGE_ROOT "/states"
#endif

#ifndef FLIP_SESSION_RECOVERY_ROOT
#define FLIP_SESSION_RECOVERY_ROOT FLIP_STORAGE_ROOT "/sessions"
#endif

#ifndef FLIP_SETTINGS_ROOT
#define FLIP_SETTINGS_ROOT FLIP_STATE_DATA_ROOT "/settings"
#endif

#ifndef FLIP_SESSION_ROOT
#define FLIP_SESSION_ROOT FLIP_STATE_DATA_ROOT "/session"
#endif

#ifndef FLIP_RECENT_ROOT
#define FLIP_RECENT_ROOT FLIP_STATE_DATA_ROOT "/recent"
#endif

#ifndef FLIP_LIBRARY_CACHE_ROOT
#define FLIP_LIBRARY_CACHE_ROOT FLIP_CACHE_ROOT
#endif

#ifndef FLIP_LIBRARY_CACHE_DIR
#define FLIP_LIBRARY_CACHE_DIR FLIP_LIBRARY_CACHE_ROOT "/library"
#endif

#ifndef FLIP_SESSION_REGISTRY_ROOT
#define FLIP_SESSION_REGISTRY_ROOT FLIP_SESSION_ROOT
#endif

#define FLIP_SETTINGS_FILE FLIP_SETTINGS_ROOT "/global"
#define FLIP_SESSION_FILE FLIP_SESSION_ROOT "/current"
#define FLIP_SESSION_REGISTRY_FILE FLIP_SESSION_REGISTRY_ROOT "/registry"
#define FLIP_RECENT_FILE FLIP_RECENT_ROOT "/history"

const char *flip_paths_storage_root(void);
const char *flip_paths_roms_root(void);
const char *flip_paths_bin_root(void);
const char *flip_paths_state_data_root(void);
const char *flip_paths_cache_root(void);
const char *flip_paths_system_root(void);
const char *flip_paths_save_root(void);
const char *flip_paths_state_root(void);
const char *flip_paths_recovery_root(void);
const char *flip_paths_settings_root(void);
const char *flip_paths_session_root(void);
const char *flip_paths_recent_root(void);

#endif
