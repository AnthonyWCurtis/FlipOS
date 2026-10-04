#include "flip_state.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int flip_state_test_build_path(
    const flip_state_context_t *context,
    const char *suffix,
    char *path,
    size_t path_size);

typedef struct {
    size_t serialize_size;
} fake_state_t;

static int fake_ok(void *userdata)
{
    (void)userdata;
    return 0;
}

static void fake_void(void *userdata)
{
    (void)userdata;
}

static int fake_load(void *userdata, const char *path)
{
    (void)userdata;
    (void)path;
    return 0;
}

static int fake_info(void *userdata, flip_core_info_t *info)
{
    (void)userdata;
    (void)info;
    return 0;
}

static int fake_input(void *userdata, const flip_game_input_t *input)
{
    (void)userdata;
    (void)input;
    return 0;
}

static size_t serialize_size(void *userdata)
{
    fake_state_t *state = userdata;
    return state->serialize_size;
}

static int fake_serialize(void *userdata, void *data, size_t size)
{
    (void)userdata;
    (void)data;
    return size > 0 ? 0 : -1;
}

static int fake_unserialize(void *userdata, const void *data, size_t size)
{
    (void)userdata;
    (void)data;
    return size > 0 ? 0 : -1;
}

static flip_core_adapter_t make_core(fake_state_t *state)
{
    flip_core_adapter_t core = {
        .userdata = state,
        .init = fake_ok,
        .deinit = fake_void,
        .load_game = fake_load,
        .unload_game = fake_void,
        .get_info = fake_info,
        .set_input = fake_input,
        .run_frame = fake_ok,
        .serialize_size = serialize_size,
        .serialize = fake_serialize,
        .unserialize = fake_unserialize,
    };

    return core;
}

static void cleanup(const flip_state_context_t *context)
{
    static const char *const suffixes[] = {
        ".state",
        ".state.prev",
        ".state.recovery"
    };

    char path[640];

    for (size_t i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); ++i) {
        if (flip_state_test_build_path(
                context,
                suffixes[i],
                path,
                sizeof(path)) == 0) {
            unlink(path);
            rmdir(path);
        }
    }

    if (snprintf(path, sizeof(path), "%s/gb", FLIP_STATE_ROOT) > 0)
        rmdir(path);

    rmdir(FLIP_STATE_ROOT);
}

static void ensure_root(void)
{
    if (mkdir(FLIP_STATE_ROOT, 0700) < 0)
        assert(errno == EEXIST);

    char path[640];
    assert(snprintf(path, sizeof(path), "%s/gb", FLIP_STATE_ROOT) > 0);

    if (mkdir(path, 0700) < 0)
        assert(errno == EEXIST);
}

static void write_bytes(
    const flip_state_context_t *context,
    const char *suffix,
    const unsigned char *data,
    size_t size)
{
    char path[640];

    assert(flip_state_test_build_path(
        context,
        suffix,
        path,
        sizeof(path)) == 0);

    FILE *file = fopen(path, "wb");
    assert(file != NULL);
    assert(fwrite(data, 1, size, file) == size);
    assert(fclose(file) == 0);
}

int main(void)
{
    flip_state_context_t context;
    flip_state_context_init(&context, "gb", "/roms/test.gb");

    cleanup(&context);
    ensure_root();

    fake_state_t fake = { .serialize_size = 4 };
    flip_core_adapter_t core = make_core(&fake);

    flip_state_status_t status;

    assert(flip_state_query_status(
        &core,
        &context,
        &status) == 0);

    assert(status.current == FLIP_STATE_HEALTH_MISSING);
    assert(status.previous == FLIP_STATE_HEALTH_MISSING);
    assert(status.recovery == FLIP_STATE_HEALTH_MISSING);

    static const unsigned char exact[] = { 1, 2, 3, 4 };
    static const unsigned char short_state[] = { 1, 2, 3 };

    write_bytes(&context, ".state", exact, sizeof(exact));
    write_bytes(&context, ".state.prev", short_state, sizeof(short_state));
    write_bytes(&context, ".state.recovery", exact, sizeof(exact));

    assert(flip_state_query_status(
        &core,
        &context,
        &status) == 0);

    assert(status.current == FLIP_STATE_HEALTH_AVAILABLE);
    assert(status.previous == FLIP_STATE_HEALTH_INVALID);
    assert(status.recovery == FLIP_STATE_HEALTH_AVAILABLE);

    cleanup(&context);

    puts("flip_state manager tests passed");
    return 0;
}
