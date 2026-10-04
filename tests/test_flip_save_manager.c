#include "flip_save.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
    unsigned char memory[32];
    size_t size;
} fake_core_t;

int flip_save_test_build_path(
    const flip_save_context_t *context,
    const char *suffix,
    char *path,
    size_t path_size);

static void *save_data(void *userdata)
{
    fake_core_t *fake = userdata;
    return fake->size ? fake->memory : NULL;
}

static size_t save_size(void *userdata)
{
    fake_core_t *fake = userdata;
    return fake->size;
}

static flip_core_adapter_t make_core(fake_core_t *fake)
{
    flip_core_adapter_t core;
    memset(&core, 0, sizeof(core));
    core.userdata = fake;
    core.save_memory_data = save_data;
    core.save_memory_size = save_size;
    return core;
}

static void cleanup_root(void)
{
    const char *files[] = {
        "/tmp/flipos-save-manager-test/gb/792f9628ba43b7d9.sav",
        "/tmp/flipos-save-manager-test/gb/792f9628ba43b7d9.sav.prev",
        "/tmp/flipos-save-manager-test/gb/792f9628ba43b7d9.sav.recovery"
    };

    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); ++i)
        unlink(files[i]);

    rmdir("/tmp/flipos-save-manager-test/gb");
    rmdir("/tmp/flipos-save-manager-test");
}

static void ensure_root(void)
{
    cleanup_root();
    assert(mkdir("/tmp/flipos-save-manager-test", 0700) == 0);
    assert(mkdir("/tmp/flipos-save-manager-test/gb", 0700) == 0);
}

static void write_bytes(
    const flip_save_context_t *context,
    const char *suffix,
    const unsigned char *data,
    size_t size)
{
    char path[512];
    assert(flip_save_test_build_path(
        context,
        suffix,
        path,
        sizeof(path)) == 0);

    FILE *f = fopen(path, "wb");
    assert(f != NULL);
    assert(fwrite(data, 1, size, f) == size);
    assert(fclose(f) == 0);
}

int main(void)
{
    ensure_root();

    fake_core_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.size = sizeof(fake.memory);

    flip_core_adapter_t core = make_core(&fake);

    flip_save_context_t context;
    flip_save_context_init(
        &context,
        "gb",
        "/mnt/SDCARD/Roms/GB/Pokemon - Yellow.gb");

    flip_save_status_t status;

    assert(flip_save_query_status(
        &core,
        &context,
        &status) == 0);

    assert(status.current == FLIP_SAVE_HEALTH_MISSING);
    assert(status.previous == FLIP_SAVE_HEALTH_MISSING);
    assert(status.recovery == FLIP_SAVE_HEALTH_MISSING);

    unsigned char good[32];
    memset(good, 0x44, sizeof(good));

    const unsigned char invalid[3] = {1, 2, 3};

    write_bytes(
        &context,
        FLIP_SAVE_CURRENT_SUFFIX,
        good,
        sizeof(good));

    write_bytes(
        &context,
        FLIP_SAVE_PREVIOUS_SUFFIX,
        invalid,
        sizeof(invalid));

    write_bytes(
        &context,
        FLIP_SAVE_RECOVERY_SUFFIX,
        good,
        sizeof(good));

    assert(flip_save_query_status(
        &core,
        &context,
        &status) == 0);

    assert(status.current == FLIP_SAVE_HEALTH_HEALTHY);
    assert(status.previous == FLIP_SAVE_HEALTH_INVALID);
    assert(status.recovery == FLIP_SAVE_HEALTH_HEALTHY);

    cleanup_root();

    puts("flip_save manager tests passed");
    return 0;
}
