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
    unlink("/tmp/flipos-power-service-test/mem_sleep");
    unlink("/tmp/flipos-power-service-test/state");
    rmdir("/tmp/flipos-power-service-test");
    assert(mkdir("/tmp/flipos-power-service-test", 0777) == 0);

    write_file(
        "/tmp/flipos-power-service-test/brightness",
        "200\n");

    write_file(
        "/tmp/flipos-power-service-test/mem_sleep",
        "s2idle [deep]\n");

    write_file(
        "/tmp/flipos-power-service-test/state",
        "freeze mem\n");

    assert(flip_power_get_brightness() == 200);
    assert(flip_power_brightness_to_percent(10) == 0);
    assert(flip_power_brightness_to_percent(255) == 100);
    assert(flip_power_percent_to_brightness(0) == 10);
    assert(flip_power_percent_to_brightness(50) == 133);
    assert(flip_power_percent_to_brightness(100) == 255);

    assert(flip_power_set_brightness(0) == 0);
    assert(read_int(
        "/tmp/flipos-power-service-test/brightness") == 10);

    assert(flip_power_set_brightness(999) == 0);
    assert(read_int(
        "/tmp/flipos-power-service-test/brightness") == 255);

    /*
     * Power Menu sleep must blank the display, select deep sleep, write mem,
     * then restore the user's previous backlight level after wake.
     */
    assert(flip_power_set_brightness(200) == 0);
    assert(flip_power_sleep_once() == 0);
    assert(read_int(
        "/tmp/flipos-power-service-test/brightness") == 200);

    FILE *mem_sleep = fopen(
        "/tmp/flipos-power-service-test/mem_sleep",
        "r");
    assert(mem_sleep != NULL);
    char mem_value[32] = {0};
    assert(fgets(mem_value, sizeof(mem_value), mem_sleep) != NULL);
    assert(fclose(mem_sleep) == 0);
    assert(strncmp(mem_value, "deep", 4) == 0);

    FILE *state_file = fopen(
        "/tmp/flipos-power-service-test/state",
        "r");
    assert(state_file != NULL);
    char state_value[32] = {0};
    assert(fgets(state_value, sizeof(state_value), state_file) != NULL);
    assert(fclose(state_file) == 0);
    assert(strncmp(state_value, "mem", 3) == 0);

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
