#include "flip_session_adapter.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    const flip_session_adapter_t *adapter =
        flip_session_adapter_for_system("GB");

    assert(adapter != NULL);
    assert(strcmp(adapter->system, "GB") == 0);
    assert(adapter->map_exit_code != NULL);
    assert(adapter->resume_available != NULL);

    assert(
        adapter->map_exit_code(20) ==
        FLIP_SESSION_ACTION_SUSPEND);

    assert(
        adapter->map_exit_code(0) ==
        FLIP_SESSION_ACTION_EXIT);

    assert(
        adapter->map_exit_code(7) ==
        FLIP_SESSION_ACTION_EXIT);

    assert(
        adapter->resume_available(
            "/definitely/not/a/real/flipos-rom.gb") == 0);

    assert(
        flip_session_adapter_for_system("UNKNOWN") == NULL);

    puts("flip_session_adapter tests passed");
    return 0;
}
