#ifndef FLIP_SETTINGS_H
#define FLIP_SETTINGS_H

typedef struct {
    int volume;
    int brightness;
} flip_settings_t;

int flip_settings_load(
    flip_settings_t *settings);

int flip_settings_save(
    const flip_settings_t *settings);

int flip_settings_get_volume(void);

int flip_settings_set_volume(
    int volume);

int flip_settings_get_brightness(void);

int flip_settings_set_brightness(
    int brightness);

/*
 * Persist dirty RAM-backed settings once the
 * configured idle period has elapsed.
 *
 * Safe to call every frame.
 *
 * Returns:
 *   1 = settings were flushed
 *   0 = nothing to do yet
 *  -1 = persistence failed
 */
int flip_settings_tick(void);

/*
 * Immediately persist dirty settings.
 *
 * Use at lifecycle boundaries such as exit,
 * suspend or shutdown.
 *
 * Returns:
 *   1 = settings were flushed
 *   0 = settings were already clean
 *  -1 = persistence failed
 */
int flip_settings_flush(void);

#endif
