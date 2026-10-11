#ifndef FLIP_SETTINGS_H
#define FLIP_SETTINGS_H

typedef struct {
    int volume;
    int brightness;
    int battery_percent_visible;
} flip_settings_t;

/*
 * Initialize the process-local Settings service.
 *
 * Idempotent: subsequent calls do not reload the file or
 * replace in-memory values.
 *
 * Returns 0 on success, -1 on load failure.
 */
int flip_settings_init(void);

int flip_settings_is_ready(void);

int flip_settings_get_volume(void);

int flip_settings_set_volume(
    int volume);

int flip_settings_get_brightness(void);

int flip_settings_set_brightness(
    int brightness);

int flip_settings_get_battery_percent_visible(void);

int flip_settings_set_battery_percent_visible(
    int visible);

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
