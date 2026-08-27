#ifndef FLIP_SESSION_MANAGER_H
#define FLIP_SESSION_MANAGER_H

#include <stddef.h>

typedef struct flip_system flip_system_t;

#ifndef FLIP_SESSION_MANAGER_ROOT
#define FLIP_SESSION_MANAGER_ROOT "/mnt/SDCARD/flipos-dev/sessions"
#endif

typedef enum {
    FLIP_RETAINED_SESSION_SUSPENDED = 0,
    FLIP_RETAINED_SESSION_INTERRUPTED = 1
} flip_retained_session_status_t;

typedef struct {
    char system[32];
    char rom[512];
    flip_retained_session_status_t status;
    long long last_active;
    int resume_available;
} flip_retained_session_t;

typedef struct {
    flip_retained_session_t *items;
    size_t count;
} flip_retained_session_list_t;

int flip_session_manager_record(
    const char *system_id,
    const char *rom_path,
    flip_retained_session_status_t status,
    long long last_active);

int flip_session_manager_record_gameswitcher(
    const flip_system_t *system,
    const char *rom_path);

int flip_session_manager_load(
    flip_retained_session_list_t *list);

int flip_session_manager_refresh(
    flip_retained_session_list_t *list);

int flip_session_manager_prune(void);

void flip_session_manager_list_free(
    flip_retained_session_list_t *list);

int flip_session_manager_remove(
    const char *system_id,
    const char *rom_path);

#endif
