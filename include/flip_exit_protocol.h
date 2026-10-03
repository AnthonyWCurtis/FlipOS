#ifndef FLIP_EXIT_PROTOCOL_H
#define FLIP_EXIT_PROTOCOL_H

/*
 * Global FlipOS emulator-process exit protocol.
 *
 * These values are process ABI between emulator executables and the
 * frontend/session manager. They are intentionally system-independent.
 */
#define FLIP_EXIT_CODE_NORMAL 0
#define FLIP_EXIT_CODE_SUSPEND 20

typedef enum {
    FLIP_EXIT_PROTOCOL_NORMAL = 0,
    FLIP_EXIT_PROTOCOL_SUSPEND,
    FLIP_EXIT_PROTOCOL_ERROR
} flip_exit_protocol_result_t;

flip_exit_protocol_result_t
flip_exit_protocol_classify(
    int exit_code);

#endif
