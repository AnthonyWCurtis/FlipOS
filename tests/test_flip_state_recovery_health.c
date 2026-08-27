#include "flip_state.h"

#include <assert.h>
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

static size_t fake_serialize_size(void *userdata)
{
    fake_state_t *state = userdata;
    return state->serialize_size;
}

static flip_core_adapter_t make_core(fake_state_t *state)
{
    flip_core_adapter_t core;
    memset(&core, 0, sizeof(core));
    core.userdata = state;
    core.serialize_size = fake_serialize_size;
    return core;
}

static void reset_root(const flip_state_context_t *context)
{
    char path[640];
    static const char *const suffixes[] = {
        ".state.recovery",
        ".state.recovery.tmp"
    };

    for (size_t i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); ++i) {
        if (flip_state_test_build_path(context, suffixes[i], path, sizeof(path)) == 0) {
            unlink(path);
            rmdir(path);
        }
    }

    snprintf(path, sizeof(path), "%s/gb", FLIP_STATE_ROOT);
    rmdir(path);
    rmdir(FLIP_STATE_ROOT);
    assert(mkdir(FLIP_STATE_ROOT, 0700) == 0);
    assert(mkdir(path, 0700) == 0);
}

static void write_bytes(
    const flip_state_context_t *context,
    const unsigned char *data,
    size_t size)
{
    char path[640];
    assert(flip_state_test_build_path(
        context, ".state.recovery", path, sizeof(path)) == 0);
    FILE *file = fopen(path, "wb");
    assert(file != NULL);
    assert(fwrite(data, 1, size, file) == size);
    assert(fclose(file) == 0);
}

int main(void)
{
    flip_state_context_t context;
    flip_state_context_init(&context, "gb", "/roms/test.gb");

    fake_state_t fake = { .serialize_size = 4 };
    flip_core_adapter_t core = make_core(&fake);

    reset_root(&context);
    assert(flip_state_recovery_health(&core, &context) ==
        FLIP_STATE_RECOVERY_MISSING);

    static const unsigned char exact[] = { 1, 2, 3, 4 };
    write_bytes(&context, exact, sizeof(exact));
    assert(flip_state_recovery_health(&core, &context) ==
        FLIP_STATE_RECOVERY_AVAILABLE);

    reset_root(&context);
    static const unsigned char short_state[] = { 1, 2, 3 };
    write_bytes(&context, short_state, sizeof(short_state));
    assert(flip_state_recovery_health(&core, &context) ==
        FLIP_STATE_RECOVERY_INVALID);

    puts("flip_state recovery health tests passed");
    return 0;
}
