#ifndef FLIP_STATE_H
#define FLIP_STATE_H

#define FLIP_STATE_ROOT "/mnt/SDCARD/flipos-dev/states"

typedef struct {
    const char *system_id;
    const char *rom_path;
    int current_was_restored;
} flip_state_context_t;

void flip_state_context_init(
    flip_state_context_t *context,
    const char *system_id,
    const char *rom_path);

#endif
