#include "flip_settings.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

static void write_settings(int volume, int brightness)
{
    assert(system("rm -rf /tmp/flipos-settings-service-test") == 0);
    assert(mkdir("/tmp/flipos-settings-service-test", 0777) == 0);
    assert(mkdir("/tmp/flipos-settings-service-test/state", 0777) == 0);
    assert(mkdir("/tmp/flipos-settings-service-test/state/settings", 0777) == 0);

    FILE *f = fopen(
        "/tmp/flipos-settings-service-test/state/settings/global",
        "w");
    assert(f != NULL);

    fprintf(f, "version=1\n");
    fprintf(f, "volume=%d\n", volume);
    fprintf(f, "brightness=%d\n", brightness);
    assert(fclose(f) == 0);
}

static void overwrite_settings(int volume, int brightness)
{
    FILE *f = fopen(
        "/tmp/flipos-settings-service-test/state/settings/global",
        "w");
    assert(f != NULL);

    fprintf(f, "version=1\n");
    fprintf(f, "volume=%d\n", volume);
    fprintf(f, "brightness=%d\n", brightness);
    assert(fclose(f) == 0);
}

int main(void)
{
    write_settings(17, 123);

    assert(flip_settings_is_ready() == 0);
    assert(flip_settings_init() == 0);
    assert(flip_settings_is_ready() == 1);
    assert(flip_settings_get_volume() == 17);
    assert(flip_settings_get_brightness() == 123);

    overwrite_settings(88, 244);

    assert(flip_settings_init() == 0);
    assert(flip_settings_get_volume() == 17);
    assert(flip_settings_get_brightness() == 123);

    assert(flip_settings_set_volume(22) == 0);
    assert(flip_settings_set_brightness(210) == 0);
    assert(flip_settings_tick() == 0);

    struct timespec wait = {
        .tv_sec = 0,
        .tv_nsec = 800000000L
    };
    assert(nanosleep(&wait, NULL) == 0);
    assert(flip_settings_tick() == 1);
    assert(flip_settings_flush() == 0);

    FILE *saved = fopen(
        "/tmp/flipos-settings-service-test/state/settings/global",
        "r");
    assert(saved != NULL);

    char persisted[256];
    size_t persisted_size = fread(
        persisted,
        1,
        sizeof(persisted) - 1,
        saved);
    assert(fclose(saved) == 0);

    persisted[persisted_size] = '\0';
    assert(strstr(persisted, "volume=22\n") != NULL);
    assert(strstr(persisted, "brightness=210\n") != NULL);

    assert(system("rm -rf /tmp/flipos-settings-service-test") == 0);

    puts("flip_settings service tests passed");
    return 0;
}
