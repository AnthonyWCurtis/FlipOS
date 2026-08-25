#include "flip_settings.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define SETTINGS_DIR \
    "/mnt/SDCARD/flipos-dev/state/settings"

#define SETTINGS_FILE \
    "/mnt/SDCARD/flipos-dev/state/settings/global"

#define DEFAULT_VOLUME 30
#define DEFAULT_BRIGHTNESS 200

/*
 * Wait this long after the most recent change
 * before committing settings to the SD card.
 */
#define SETTINGS_SAVE_DELAY_MS 750

static int current_volume =
    DEFAULT_VOLUME;

static int current_brightness =
    DEFAULT_BRIGHTNESS;

static int settings_dirty = 0;

static struct timespec dirty_since;

static int clamp_volume(
    int volume)
{
    if (volume < 0)
        return 0;

    if (volume > 100)
        return 100;

    return volume;
}

static int clamp_brightness(
    int brightness)
{
    if (brightness < 10)
        return 10;

    if (brightness > 255)
        return 255;

    return brightness;
}

static int ensure_settings_dir(void)
{
    if (mkdir(
            "/mnt/SDCARD/flipos-dev/state",
            0777) < 0 &&
        errno != EEXIST) {

        return -1;
    }

    if (mkdir(
            SETTINGS_DIR,
            0777) < 0 &&
        errno != EEXIST) {

        return -1;
    }

    return 0;
}

static void mark_dirty(void)
{
    settings_dirty = 1;

    clock_gettime(
        CLOCK_MONOTONIC,
        &dirty_since);
}

static long long elapsed_ms(
    const struct timespec *start,
    const struct timespec *end)
{
    long long sec =
        (long long)end->tv_sec -
        (long long)start->tv_sec;

    long long nsec =
        (long long)end->tv_nsec -
        (long long)start->tv_nsec;

    return
        sec * 1000LL +
        nsec / 1000000LL;
}

int flip_settings_load(
    flip_settings_t *settings)
{
    if (!settings)
        return -1;

    settings->volume =
        DEFAULT_VOLUME;

    settings->brightness =
        DEFAULT_BRIGHTNESS;

    FILE *f =
        fopen(
            SETTINGS_FILE,
            "r");

    if (!f) {

        if (errno == ENOENT) {

            current_volume =
                DEFAULT_VOLUME;

            current_brightness =
                DEFAULT_BRIGHTNESS;

            settings_dirty = 0;

            return 0;
        }

        return -1;
    }

    char line[128];

    while (fgets(
               line,
               sizeof(line),
               f)) {

        int volume;

        if (sscanf(
                line,
                "volume=%d",
                &volume) == 1) {

            settings->volume =
                clamp_volume(
                    volume);

            continue;
        }

        int brightness;

        if (sscanf(
                line,
                "brightness=%d",
                &brightness) == 1) {

            settings->brightness =
                clamp_brightness(
                    brightness);
        }
    }

    fclose(f);

    current_volume =
        settings->volume;

    current_brightness =
        settings->brightness;

    settings_dirty = 0;

    fprintf(
        stderr,
        "SETTINGS: loaded volume=%d brightness=%d\n",
        current_volume,
        current_brightness);

    return 1;
}

int flip_settings_save(
    const flip_settings_t *settings)
{
    if (!settings)
        return -1;

    if (ensure_settings_dir() < 0) {

        fprintf(
            stderr,
            "SETTINGS: mkdir failed: %s\n",
            strerror(errno));

        return -1;
    }

    char tmp_path[512];

    snprintf(
        tmp_path,
        sizeof(tmp_path),
        "%s.tmp",
        SETTINGS_FILE);

    FILE *f =
        fopen(
            tmp_path,
            "w");

    if (!f) {

        fprintf(
            stderr,
            "SETTINGS: fopen failed: %s\n",
            strerror(errno));

        return -1;
    }

    fprintf(
        f,
        "version=1\n");

    fprintf(
        f,
        "volume=%d\n",
        clamp_volume(
            settings->volume));

    fprintf(
        f,
        "brightness=%d\n",
        clamp_brightness(
            settings->brightness));

    fflush(f);

    if (fsync(
            fileno(f)) < 0) {

        fprintf(
            stderr,
            "SETTINGS: fsync failed: %s\n",
            strerror(errno));

        fclose(f);
        unlink(tmp_path);

        return -1;
    }

    if (fclose(f) != 0) {

        fprintf(
            stderr,
            "SETTINGS: fclose failed: %s\n",
            strerror(errno));

        unlink(tmp_path);

        return -1;
    }

    if (rename(
            tmp_path,
            SETTINGS_FILE) < 0) {

        fprintf(
            stderr,
            "SETTINGS: rename failed: %s\n",
            strerror(errno));

        unlink(tmp_path);

        return -1;
    }

    current_volume =
        clamp_volume(
            settings->volume);

    current_brightness =
        clamp_brightness(
            settings->brightness);

    settings_dirty = 0;

    fprintf(
        stderr,
        "SETTINGS: saved volume=%d brightness=%d\n",
        current_volume,
        current_brightness);

    return 0;
}

int flip_settings_get_volume(void)
{
    return current_volume;
}

int flip_settings_set_volume(
    int volume)
{
    volume =
        clamp_volume(
            volume);

    if (volume ==
        current_volume)
        return 0;

    current_volume =
        volume;

    mark_dirty();

    fprintf(
        stderr,
        "SETTINGS: volume=%d dirty\n",
        current_volume);

    return 0;
}

int flip_settings_get_brightness(void)
{
    return current_brightness;
}

int flip_settings_set_brightness(
    int brightness)
{
    brightness =
        clamp_brightness(
            brightness);

    if (brightness ==
        current_brightness)
        return 0;

    current_brightness =
        brightness;

    mark_dirty();

    fprintf(
        stderr,
        "SETTINGS: brightness=%d dirty\n",
        current_brightness);

    return 0;
}

int flip_settings_flush(void)
{
    if (!settings_dirty)
        return 0;

    flip_settings_t settings = {
        .volume =
            current_volume,

        .brightness =
            current_brightness
    };

    if (flip_settings_save(
            &settings) < 0) {

        return -1;
    }

    return 1;
}

int flip_settings_tick(void)
{
    if (!settings_dirty)
        return 0;

    struct timespec now;

    clock_gettime(
        CLOCK_MONOTONIC,
        &now);

    if (elapsed_ms(
            &dirty_since,
            &now) <
        SETTINGS_SAVE_DELAY_MS) {

        return 0;
    }

    return flip_settings_flush();
}
