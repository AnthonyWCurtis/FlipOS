#include "flip_power.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static flip_input_state_t stub_state;
static int stub_poll_result = 0;

int flip_input_poll(
    flip_input_state_t *state,
    int timeout_ms)
{
    (void)timeout_ms;

    if (!state)
        return -1;

    if (stub_poll_result < 0)
        return -1;

    *state = stub_state;
    return stub_poll_result;
}

static void write_file(
    const char *path,
    const char *value)
{
    FILE *f = fopen(path, "w");
    assert(f != NULL);
    fputs(value, f);
    assert(fclose(f) == 0);
}

static int read_int(const char *path)
{
    FILE *f = fopen(path, "r");
    assert(f != NULL);

    int value = -1;
    assert(fscanf(f, "%d", &value) == 1);
    assert(fclose(f) == 0);

    return value;
}

int main(void)
{
    unlink("/tmp/flipos-power-service-test/brightness");
    rmdir("/tmp/flipos-power-service-test");
    assert(mkdir("/tmp/flipos-power-service-test", 0777) == 0);

    write_file(
        "/tmp/flipos-power-service-test/brightness",
        "200\n");

    assert(flip_power_get_brightness() == 200);

    assert(flip_power_set_brightness(0) == 0);
    assert(read_int(
        "/tmp/flipos-power-service-test/brightness") == 10);

    assert(flip_power_set_brightness(999) == 0);
    assert(read_int(
        "/tmp/flipos-power-service-test/brightness") == 255);

    memset(&stub_state, 0, sizeof(stub_state));
    stub_poll_result = -1;

    assert(flip_power_refresh_lid_state(
        &stub_state) == FLIP_POWER_LID_ERROR);

    stub_poll_result = 0;

    assert(flip_power_refresh_lid_state(
        &stub_state) == FLIP_POWER_LID_UNKNOWN);

    stub_state.lid_ready = 1;
    stub_state.lid_closed = 0;
    assert(flip_power_refresh_lid_state(
        &stub_state) == FLIP_POWER_LID_OPEN);

    stub_state.lid_ready = 1;
    stub_state.lid_closed = 1;
    assert(flip_power_refresh_lid_state(
        &stub_state) == FLIP_POWER_LID_CLOSED);

    unlink("/tmp/flipos-power-service-test/brightness");
    rmdir("/tmp/flipos-power-service-test");

    puts("flip_power service tests passed");
    return 0;
}
