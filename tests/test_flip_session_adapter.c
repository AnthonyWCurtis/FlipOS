#include "flip_session_adapter.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void check_adapter(const char *system, const char *missing_rom)
{
    const flip_session_adapter_t *adapter =
        flip_session_adapter_for_system(system);

    assert(adapter != NULL);
    assert(strcmp(adapter->system, system) == 0);
    assert(adapter->resume_available != NULL);
    assert(adapter->resume_available(missing_rom) == 0);
    assert(adapter->supports_save_safety != NULL);
    assert(adapter->supports_save_safety() == 1);
}

int main(void)
{
    check_adapter("GB", "/definitely/not/a/real/flipos-rom.gb");
    check_adapter("GBC", "/definitely/not/a/real/flipos-rom.gbc");
    check_adapter("GBA", "/definitely/not/a/real/flipos-rom.gba");

    assert(flip_session_adapter_for_system("UNKNOWN") == NULL);

    puts("flip_session_adapter tests passed");
    return 0;
}
