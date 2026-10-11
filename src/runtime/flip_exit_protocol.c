#include "flip_exit_protocol.h"

flip_exit_protocol_result_t
flip_exit_protocol_classify(
    int exit_code)
{
    if (exit_code == FLIP_EXIT_CODE_NORMAL)
        return FLIP_EXIT_PROTOCOL_NORMAL;

    if (exit_code == FLIP_EXIT_CODE_SUSPEND)
        return FLIP_EXIT_PROTOCOL_SUSPEND;

    if (exit_code == FLIP_EXIT_CODE_LOW_BATTERY)
        return FLIP_EXIT_PROTOCOL_LOW_BATTERY;

    return FLIP_EXIT_PROTOCOL_ERROR;
}
