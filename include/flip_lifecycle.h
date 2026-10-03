#ifndef FLIP_LIFECYCLE_H
#define FLIP_LIFECYCLE_H

#include "flip_controls.h"
#include "flip_exit_protocol.h"
#include "flip_timing.h"

typedef enum {
    FLIP_LIFECYCLE_REASON_NONE = 0,
    FLIP_LIFECYCLE_REASON_SUSPEND,
    FLIP_LIFECYCLE_REASON_GAMESWITCHER,
    FLIP_LIFECYCLE_REASON_EXIT
} flip_lifecycle_reason_t;

typedef enum {
    FLIP_LIFECYCLE_CHECKPOINT_IMMEDIATE = 0,
    FLIP_LIFECYCLE_CHECKPOINT_AFTER_FRAME
} flip_lifecycle_checkpoint_t;

typedef int (*flip_lifecycle_save_fn)(void *userdata);

typedef struct {
    flip_lifecycle_reason_t reason;
    int lid_changed;
    int lid_closed;
    int save_attempted;
    int save_succeeded;
    int should_exit;
    int exit_code;
} flip_lifecycle_result_t;


int flip_lifecycle_handle(
    const flip_system_actions_t *actions,
    flip_lifecycle_checkpoint_t checkpoint,
    flip_lifecycle_save_fn save_fn,
    void *userdata,
    flip_lifecycle_result_t *result);

int flip_lifecycle_resume_timing(
    flip_timing_t *timing);

int flip_lifecycle_shutdown_shared(void);

#endif
