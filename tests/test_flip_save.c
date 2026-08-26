#include "flip_save.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

int flip_save_test_build_path(
    const flip_save_context_t *context,
    const char *suffix,
    char *path,
    size_t path_size);

typedef struct {
    unsigned char save_memory[32];
    size_t save_memory_size;
} fake_save_core_t;

static void *fake_save_memory_data(void *userdata)
{
    fake_save_core_t *fake = userdata;
    return fake->save_memory_size > 0 ? fake->save_memory : NULL;
}

static size_t fake_save_memory_size(void *userdata)
{
    fake_save_core_t *fake = userdata;
    return fake->save_memory_size;
}

static flip_core_adapter_t make_core(fake_save_core_t *fake)
{
    flip_core_adapter_t core;
    memset(&core, 0, sizeof(core));
    core.userdata = fake;
    core.save_memory_data = fake_save_memory_data;
    core.save_memory_size = fake_save_memory_size;
    return core;
}

static void reset_root(void)
{
    char command[768];
    int written = snprintf(command, sizeof(command), "rm -rf '%s'", FLIP_SAVE_ROOT);
    assert(written > 0 && (size_t)written < sizeof(command));
    assert(system(command) == 0);
    assert(mkdir(FLIP_SAVE_ROOT, 0700) == 0);

    char system_dir[640];
    written = snprintf(system_dir, sizeof(system_dir), "%s/gb", FLIP_SAVE_ROOT);
    assert(written > 0 && (size_t)written < sizeof(system_dir));
    assert(mkdir(system_dir, 0700) == 0);
}

static void write_bytes(
    const flip_save_context_t *context,
    const char *suffix,
    const unsigned char *data,
    size_t size)
{
    char path[640];
    assert(flip_save_test_build_path(context, suffix, path, sizeof(path)) == 0);
    FILE *file = fopen(path, "wb");
    assert(file != NULL);
    assert(fwrite(data, 1, size, file) == size);
    assert(fclose(file) == 0);
}

static void read_bytes(
    const flip_save_context_t *context,
    const char *suffix,
    unsigned char *data,
    size_t size)
{
    char path[640];
    assert(flip_save_test_build_path(context, suffix, path, sizeof(path)) == 0);
    FILE *file = fopen(path, "rb");
    assert(file != NULL);
    assert(fread(data, 1, size, file) == size);
    assert(fgetc(file) == EOF);
    assert(fclose(file) == 0);
}

static int path_exists(
    const flip_save_context_t *context,
    const char *suffix)
{
    char path[640];
    struct stat st;
    assert(flip_save_test_build_path(context, suffix, path, sizeof(path)) == 0);
    return stat(path, &st) == 0;
}

static void test_paths_use_frozen_rom_identity(void)
{
    flip_save_context_t context;
    char path[512];

    flip_save_context_init(
        &context,
        "gb",
        "/mnt/SDCARD/Roms/GB/Pokemon - Yellow.gb");

    assert(flip_save_test_build_path(&context, ".sav", path, sizeof(path)) == 0);
    assert(strcmp(path, FLIP_SAVE_ROOT "/gb/792f9628ba43b7d9.sav") == 0);

    assert(flip_save_test_build_path(&context, ".sav.prev", path, sizeof(path)) == 0);
    assert(strcmp(path, FLIP_SAVE_ROOT "/gb/792f9628ba43b7d9.sav.prev") == 0);

    assert(flip_save_test_build_path(&context, ".sav.tmp", path, sizeof(path)) == 0);
    assert(strcmp(path, FLIP_SAVE_ROOT "/gb/792f9628ba43b7d9.sav.tmp") == 0);

    assert(flip_save_test_build_path(&context, ".sav.prev.tmp", path, sizeof(path)) == 0);
    assert(strcmp(path, FLIP_SAVE_ROOT "/gb/792f9628ba43b7d9.sav.prev.tmp") == 0);
}

static void test_no_save_memory_is_noop(void)
{
    reset_root();

    fake_save_core_t fake;
    memset(&fake, 0, sizeof(fake));
    flip_core_adapter_t core = make_core(&fake);

    flip_save_context_t context;
    flip_save_context_init(&context, "gb", "/mnt/SDCARD/Roms/GB/no-battery.gb");

    assert(flip_save_load(&core, &context) == FLIP_SAVE_LOAD_NO_MEMORY);
    assert(flip_save_commit(&core, &context) == 0);
}

static void test_exact_size_current_is_loaded(void)
{
    reset_root();

    fake_save_core_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.save_memory_size = sizeof(fake.save_memory);

    unsigned char saved[32];
    for (size_t i = 0; i < sizeof(saved); ++i)
        saved[i] = (unsigned char)(0x40 + i);

    flip_save_context_t context;
    flip_save_context_init(&context, "gb", "/mnt/SDCARD/Roms/GB/Pokemon - Yellow.gb");
    write_bytes(&context, ".sav", saved, sizeof(saved));

    flip_core_adapter_t core = make_core(&fake);
    assert(flip_save_load(&core, &context) == FLIP_SAVE_LOAD_CURRENT);
    assert(memcmp(fake.save_memory, saved, sizeof(saved)) == 0);
}

static void test_invalid_current_falls_back_to_previous(void)
{
    reset_root();

    fake_save_core_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.save_memory_size = sizeof(fake.save_memory);

    const unsigned char bad_current[3] = {1, 2, 3};
    unsigned char previous[32];
    for (size_t i = 0; i < sizeof(previous); ++i)
        previous[i] = (unsigned char)(0x80 + i);

    flip_save_context_t context;
    flip_save_context_init(&context, "gb", "/mnt/SDCARD/Roms/GB/Pokemon - Yellow.gb");
    write_bytes(&context, ".sav", bad_current, sizeof(bad_current));
    write_bytes(&context, ".sav.prev", previous, sizeof(previous));

    flip_core_adapter_t core = make_core(&fake);
    assert(flip_save_load(&core, &context) == FLIP_SAVE_LOAD_PREVIOUS);
    assert(memcmp(fake.save_memory, previous, sizeof(previous)) == 0);
}

static void test_initial_commit_creates_current_save(void)
{
    reset_root();

    fake_save_core_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.save_memory_size = sizeof(fake.save_memory);
    for (size_t i = 0; i < sizeof(fake.save_memory); ++i)
        fake.save_memory[i] = (unsigned char)(0x10 + i);

    flip_save_context_t context;
    flip_save_context_init(&context, "gb", "/mnt/SDCARD/Roms/GB/Pokemon - Yellow.gb");
    flip_core_adapter_t core = make_core(&fake);

    assert(flip_save_commit(&core, &context) == 0);
    assert(path_exists(&context, ".sav"));
    assert(!path_exists(&context, ".sav.prev"));

    unsigned char current[32];
    read_bytes(&context, ".sav", current, sizeof(current));
    assert(memcmp(current, fake.save_memory, sizeof(current)) == 0);
}

static void test_commit_preserves_valid_current_as_previous(void)
{
    reset_root();

    unsigned char original[32];
    unsigned char replacement[32];
    for (size_t i = 0; i < sizeof(original); ++i) {
        original[i] = (unsigned char)(0x20 + i);
        replacement[i] = (unsigned char)(0x60 + i);
    }

    flip_save_context_t context;
    flip_save_context_init(&context, "gb", "/mnt/SDCARD/Roms/GB/Pokemon - Yellow.gb");
    write_bytes(&context, ".sav", original, sizeof(original));

    fake_save_core_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.save_memory_size = sizeof(fake.save_memory);
    memcpy(fake.save_memory, replacement, sizeof(replacement));
    flip_core_adapter_t core = make_core(&fake);

    assert(flip_save_commit(&core, &context) == 0);

    unsigned char current[32];
    unsigned char previous[32];
    read_bytes(&context, ".sav", current, sizeof(current));
    read_bytes(&context, ".sav.prev", previous, sizeof(previous));
    assert(memcmp(current, replacement, sizeof(current)) == 0);
    assert(memcmp(previous, original, sizeof(previous)) == 0);
}

static void test_invalid_current_does_not_replace_existing_previous(void)
{
    reset_root();

    const unsigned char invalid_current[3] = {9, 8, 7};
    unsigned char protected_previous[32];
    for (size_t i = 0; i < sizeof(protected_previous); ++i)
        protected_previous[i] = (unsigned char)(0x90 + i);

    flip_save_context_t context;
    flip_save_context_init(&context, "gb", "/mnt/SDCARD/Roms/GB/Pokemon - Yellow.gb");
    write_bytes(&context, ".sav", invalid_current, sizeof(invalid_current));
    write_bytes(&context, ".sav.prev", protected_previous, sizeof(protected_previous));

    fake_save_core_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.save_memory_size = sizeof(fake.save_memory);
    memset(fake.save_memory, 0x55, sizeof(fake.save_memory));
    flip_core_adapter_t core = make_core(&fake);

    assert(flip_save_commit(&core, &context) == 0);

    unsigned char previous[32];
    read_bytes(&context, ".sav.prev", previous, sizeof(previous));
    assert(memcmp(previous, protected_previous, sizeof(previous)) == 0);
}

static void test_tmp_open_failure_preserves_current_and_previous(void)
{
    reset_root();

    unsigned char current_before[32];
    unsigned char previous_before[32];
    for (size_t i = 0; i < sizeof(current_before); ++i) {
        current_before[i] = (unsigned char)(0x30 + i);
        previous_before[i] = (unsigned char)(0x70 + i);
    }

    flip_save_context_t context;
    flip_save_context_init(&context, "gb", "/mnt/SDCARD/Roms/GB/Pokemon - Yellow.gb");
    write_bytes(&context, ".sav", current_before, sizeof(current_before));
    write_bytes(&context, ".sav.prev", previous_before, sizeof(previous_before));

    char tmp_path[640];
    assert(flip_save_test_build_path(&context, ".sav.tmp", tmp_path, sizeof(tmp_path)) == 0);
    assert(mkdir(tmp_path, 0700) == 0);

    fake_save_core_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.save_memory_size = sizeof(fake.save_memory);
    memset(fake.save_memory, 0xa5, sizeof(fake.save_memory));
    flip_core_adapter_t core = make_core(&fake);

    assert(flip_save_commit(&core, &context) == -1);

    unsigned char current_after[32];
    unsigned char previous_after[32];
    read_bytes(&context, ".sav", current_after, sizeof(current_after));
    read_bytes(&context, ".sav.prev", previous_after, sizeof(previous_after));
    assert(memcmp(current_after, current_before, sizeof(current_after)) == 0);
    assert(memcmp(previous_after, previous_before, sizeof(previous_after)) == 0);
}

int main(void)
{
    test_paths_use_frozen_rom_identity();
    test_no_save_memory_is_noop();
    test_exact_size_current_is_loaded();
    test_invalid_current_falls_back_to_previous();
    test_initial_commit_creates_current_save();
    test_commit_preserves_valid_current_as_previous();
    test_invalid_current_does_not_replace_existing_previous();
    test_tmp_open_failure_preserves_current_and_previous();

    puts("flip_save tests passed");
    return 0;
}
