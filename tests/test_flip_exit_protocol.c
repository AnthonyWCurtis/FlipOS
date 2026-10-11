#include "flip_exit_protocol.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    assert(
        flip_exit_protocol_classify(
            FLIP_EXIT_CODE_NORMAL) ==
        FLIP_EXIT_PROTOCOL_NORMAL);

    assert(
        flip_exit_protocol_classify(
            FLIP_EXIT_CODE_SUSPEND) ==
        FLIP_EXIT_PROTOCOL_SUSPEND);

    assert(
        flip_exit_protocol_classify(
            FLIP_EXIT_CODE_LOW_BATTERY) ==
        FLIP_EXIT_PROTOCOL_LOW_BATTERY);

    assert(
        flip_exit_protocol_classify(1) ==
        FLIP_EXIT_PROTOCOL_ERROR);

    assert(
        flip_exit_protocol_classify(7) ==
        FLIP_EXIT_PROTOCOL_ERROR);

    assert(
        flip_exit_protocol_classify(127) ==
        FLIP_EXIT_PROTOCOL_ERROR);

    assert(
        flip_exit_protocol_classify(255) ==
        FLIP_EXIT_PROTOCOL_ERROR);

    puts("flip_exit_protocol tests passed");
    return 0;
}
