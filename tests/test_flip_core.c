#include "flip_core.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    int init_calls;
    int deinit_calls;
    int load_calls;
    int unload_calls;
    int info_calls;
    int input_calls;
    int frame_calls;
    int serialize_size_calls;
    int serialize_calls;
    int unserialize_calls;
    int save_memory_data_calls;
    int save_memory_size_calls;

    char loaded_path[128];
    flip_game_input_t last_input;
    uint8_t state[8];
    uint8_t save_memory[32];
} fake_core_t;

static int fake_init(void *userdata)
{
    fake_core_t *fake = userdata;
    fake->init_calls++;
    return 0;
}

static void fake_deinit(void *userdata)
{
    fake_core_t *fake = userdata;
    fake->deinit_calls++;
}

static int fake_load(void *userdata, const char *path)
{
    fake_core_t *fake = userdata;
    fake->load_calls++;
    snprintf(fake->loaded_path, sizeof(fake->loaded_path), "%s", path);
    return 0;
}

static void fake_unload(void *userdata)
{
    fake_core_t *fake = userdata;
    fake->unload_calls++;
}

static int fake_info(void *userdata, flip_core_info_t *info)
{
    fake_core_t *fake = userdata;
    fake->info_calls++;

    memset(info, 0, sizeof(*info));
    info->core_name = "fake";
    info->core_version = "1.0";
    info->target_fps = 60.0;
    info->audio_sample_rate = 32768;
    info->base_width = 160;
    info->base_height = 144;
    return 0;
}

static int fake_input(void *userdata, const flip_game_input_t *input)
{
    fake_core_t *fake = userdata;
    fake->input_calls++;
    fake->last_input = *input;
    return 0;
}

static int fake_frame(void *userdata)
{
    fake_core_t *fake = userdata;
    fake->frame_calls++;
    return 0;
}

static size_t fake_serialize_size(void *userdata)
{
    fake_core_t *fake = userdata;
    fake->serialize_size_calls++;
    return sizeof(fake->state);
}

static int fake_serialize(void *userdata, void *data, size_t size)
{
    fake_core_t *fake = userdata;
    fake->serialize_calls++;

    if (size != sizeof(fake->state))
        return -1;

    memcpy(data, fake->state, size);
    return 0;
}

static int fake_unserialize(void *userdata, const void *data, size_t size)
{
    fake_core_t *fake = userdata;
    fake->unserialize_calls++;

    if (size != sizeof(fake->state))
        return -1;

    memcpy(fake->state, data, size);
    return 0;
}

static void *fake_save_memory_data(void *userdata)
{
    fake_core_t *fake = userdata;
    fake->save_memory_data_calls++;
    return fake->save_memory;
}

static size_t fake_save_memory_size(void *userdata)
{
    fake_core_t *fake = userdata;
    fake->save_memory_size_calls++;
    return sizeof(fake->save_memory);
}

static flip_core_adapter_t make_adapter(fake_core_t *fake)
{
    flip_core_adapter_t adapter = {
        .userdata = fake,
        .init = fake_init,
        .deinit = fake_deinit,
        .load_game = fake_load,
        .unload_game = fake_unload,
        .get_info = fake_info,
        .set_input = fake_input,
        .run_frame = fake_frame,
        .serialize_size = fake_serialize_size,
        .serialize = fake_serialize,
        .unserialize = fake_unserialize,
        .save_memory_data = fake_save_memory_data,
        .save_memory_size = fake_save_memory_size,
    };

    return adapter;
}

static void test_complete_adapter_forwards_calls(void)
{
    fake_core_t fake;
    memset(&fake, 0, sizeof(fake));

    flip_core_adapter_t adapter = make_adapter(&fake);

    assert(flip_core_validate(&adapter) == 0);
    assert(flip_core_init(&adapter) == 0);
    assert(flip_core_load_game(&adapter, "/roms/test.gb") == 0);

    flip_core_info_t info;
    assert(flip_core_get_info(&adapter, &info) == 0);
    assert(strcmp(info.core_name, "fake") == 0);
    assert(info.target_fps == 60.0);
    assert(info.audio_sample_rate == 32768);

    flip_game_input_t input;
    memset(&input, 0, sizeof(input));
    input.down[FLIP_GAME_A] = 1;
    input.lx = 123;

    assert(flip_core_set_input(&adapter, &input) == 0);
    assert(flip_core_run_frame(&adapter) == 0);

    assert(fake.init_calls == 1);
    assert(fake.load_calls == 1);
    assert(strcmp(fake.loaded_path, "/roms/test.gb") == 0);
    assert(fake.info_calls == 1);
    assert(fake.input_calls == 1);
    assert(fake.last_input.down[FLIP_GAME_A] == 1);
    assert(fake.last_input.lx == 123);
    assert(fake.frame_calls == 1);

    flip_core_unload_game(&adapter);
    flip_core_deinit(&adapter);

    assert(fake.unload_calls == 1);
    assert(fake.deinit_calls == 1);
}

static void test_serialization_forwards_exact_bytes(void)
{
    fake_core_t fake;
    memset(&fake, 0, sizeof(fake));

    for (size_t i = 0; i < sizeof(fake.state); ++i)
        fake.state[i] = (uint8_t)(i + 1);

    flip_core_adapter_t adapter = make_adapter(&fake);

    size_t size = flip_core_serialize_size(&adapter);
    assert(size == sizeof(fake.state));

    uint8_t saved[8];
    memset(saved, 0, sizeof(saved));

    assert(flip_core_serialize(&adapter, saved, sizeof(saved)) == 0);
    assert(memcmp(saved, fake.state, sizeof(saved)) == 0);

    uint8_t replacement[8] = {8, 7, 6, 5, 4, 3, 2, 1};
    assert(flip_core_unserialize(&adapter, replacement, sizeof(replacement)) == 0);
    assert(memcmp(fake.state, replacement, sizeof(replacement)) == 0);

    assert(fake.serialize_size_calls == 1);
    assert(fake.serialize_calls == 1);
    assert(fake.unserialize_calls == 1);
}

static void test_battery_memory_forwards_pointer_and_size(void)
{
    fake_core_t fake;
    memset(&fake, 0, sizeof(fake));

    flip_core_adapter_t adapter = make_adapter(&fake);

    assert(flip_core_validate(&adapter) == 0);
    assert(flip_core_save_memory_data(&adapter) == fake.save_memory);
    assert(flip_core_save_memory_size(&adapter) == sizeof(fake.save_memory));
    assert(fake.save_memory_data_calls == 1);
    assert(fake.save_memory_size_calls == 1);
}

static void test_battery_memory_is_optional(void)
{
    fake_core_t fake;
    memset(&fake, 0, sizeof(fake));

    flip_core_adapter_t adapter = make_adapter(&fake);
    adapter.save_memory_data = NULL;
    adapter.save_memory_size = NULL;

    assert(flip_core_validate(&adapter) == 0);
    assert(flip_core_save_memory_data(&adapter) == NULL);
    assert(flip_core_save_memory_size(&adapter) == 0);
}

static void test_missing_required_operation_is_rejected(void)
{
    fake_core_t fake;
    memset(&fake, 0, sizeof(fake));

    flip_core_adapter_t adapter = make_adapter(&fake);
    adapter.run_frame = NULL;

    assert(flip_core_validate(&adapter) < 0);
    assert(flip_core_run_frame(&adapter) < 0);
}

static void test_invalid_arguments_are_rejected(void)
{
    fake_core_t fake;
    memset(&fake, 0, sizeof(fake));

    flip_core_adapter_t adapter = make_adapter(&fake);

    assert(flip_core_validate(NULL) < 0);
    assert(flip_core_load_game(&adapter, NULL) < 0);
    assert(flip_core_get_info(&adapter, NULL) < 0);
    assert(flip_core_set_input(&adapter, NULL) < 0);
    assert(flip_core_serialize(&adapter, NULL, sizeof(fake.state)) < 0);
    assert(flip_core_unserialize(&adapter, NULL, sizeof(fake.state)) < 0);
    assert(flip_core_save_memory_data(NULL) == NULL);
    assert(flip_core_save_memory_size(NULL) == 0);
}

int main(void)
{
    test_complete_adapter_forwards_calls();
    test_serialization_forwards_exact_bytes();
    test_battery_memory_forwards_pointer_and_size();
    test_battery_memory_is_optional();
    test_missing_required_operation_is_rejected();
    test_invalid_arguments_are_rejected();

    puts("flip_core tests passed");
    return 0;
}
