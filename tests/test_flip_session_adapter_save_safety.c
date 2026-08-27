#include "flip_session_adapter.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

static void test_gb_save_safety_command_builders(void)
{
    const flip_session_adapter_t *adapter =
        flip_session_adapter_for_system("GB");

    assert(adapter != NULL);
    assert(adapter->supports_save_safety != NULL);
    assert(adapter->supports_save_safety() == 1);
    assert(adapter->build_start_normal_argv != NULL);
    assert(adapter->build_save_safety_argv != NULL);

    char *argv[5] = {0};

    assert(adapter->build_start_normal_argv(
        "/mnt/SDCARD/flipos-dev/bin/flip-gb",
        "/mnt/SDCARD/Roms/GB/Pokemon - Yellow.gb",
        argv,
        5) == 0);
    assert(strcmp(argv[0], "/mnt/SDCARD/flipos-dev/bin/flip-gb") == 0);
    assert(strcmp(argv[1], "/mnt/SDCARD/Roms/GB/Pokemon - Yellow.gb") == 0);
    assert(strcmp(argv[2], "--no-resume") == 0);
    assert(argv[3] == NULL);

    memset(argv, 0, sizeof(argv));
    assert(adapter->build_save_safety_argv(
        "/mnt/SDCARD/flipos-dev/bin/flip-gb",
        "/mnt/SDCARD/Roms/GB/Pokemon - Yellow.gb",
        "status",
        argv,
        5) == 0);
    assert(strcmp(argv[2], "--save-safety-status") == 0);
    assert(argv[3] == NULL);

    memset(argv, 0, sizeof(argv));
    assert(adapter->build_save_safety_argv(
        "/mnt/SDCARD/flipos-dev/bin/flip-gb",
        "/mnt/SDCARD/Roms/GB/Pokemon - Yellow.gb",
        "restore-previous",
        argv,
        5) == 0);
    assert(strcmp(argv[2], "--restore-previous-save") == 0);

    memset(argv, 0, sizeof(argv));
    assert(adapter->build_save_safety_argv(
        "/mnt/SDCARD/flipos-dev/bin/flip-gb",
        "/mnt/SDCARD/Roms/GB/Pokemon - Yellow.gb",
        "undo",
        argv,
        5) == 0);
    assert(strcmp(argv[2], "--undo-last-restore") == 0);

    assert(adapter->build_save_safety_argv(
        "/runner", "/rom.gb", "unknown", argv, 5) < 0);
}

static void test_gbc_uses_same_generic_capability(void)
{
    const flip_session_adapter_t *adapter =
        flip_session_adapter_for_system("GBC");

    assert(adapter != NULL);
    assert(adapter->supports_save_safety != NULL);
    assert(adapter->supports_save_safety() == 1);
}

int main(void)
{
    test_gb_save_safety_command_builders();
    test_gbc_uses_same_generic_capability();
    return 0;
}
