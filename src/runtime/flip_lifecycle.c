#include "flip_lifecycle.h"

#include "flip_audio.h"
#include "flip_input.h"
#include "flip_settings.h"
#include "flip_video.h"

#include <stdio.h>
#include <string.h>

static int run_save(
    flip_lifecycle_save_fn save_fn,
    void *userdata,
    flip_lifecycle_result_t *result)
{
    result->save_attempted = 1;

    if (!save_fn) {
        result->save_succeeded = 0;
        return -1;
    }

    int rc =
        save_fn(userdata);

    result->save_succeeded =
        rc >= 0;

    return result->save_succeeded
        ? 0
        : -1;
}

static const char *reason_name(
    flip_lifecycle_reason_t reason)
{
    switch (reason) {
        case FLIP_LIFECYCLE_REASON_SUSPEND:
            return "suspend";

        case FLIP_LIFECYCLE_REASON_GAMESWITCHER:
            return "gameswitcher";

        case FLIP_LIFECYCLE_REASON_GAME_OPTIONS:
            return "game_options";

        case FLIP_LIFECYCLE_REASON_EXIT:
            return "exit";

        case FLIP_LIFECYCLE_REASON_NONE:
        default:
            return "none";
    }
}

static int handle_save_to_switcher(
    flip_lifecycle_reason_t reason,
    flip_lifecycle_save_fn save_fn,
    void *userdata,
    flip_lifecycle_result_t *result)
{
    result->reason = reason;

    fprintf(
        stderr,
        "FLIP_LIFECYCLE: %s requested\n",
        reason_name(reason));

    if (run_save(
            save_fn,
            userdata,
            result) < 0) {

        fprintf(
            stderr,
            "FLIP_LIFECYCLE: %s save failed\n",
            reason_name(reason));

        return 0;
    }

    fprintf(
        stderr,
        "FLIP_LIFECYCLE: %s save ok\n",
        reason_name(reason));

    result->should_exit = 1;
    result->exit_code =
        FLIP_LIFECYCLE_EXIT_SWITCHER;

    return 1;
}

int flip_lifecycle_handle(
    const flip_system_actions_t *actions,
    flip_lifecycle_checkpoint_t checkpoint,
    flip_lifecycle_save_fn save_fn,
    void *userdata,
    flip_lifecycle_result_t *result)
{
    if (!actions ||
        !result ||
        (checkpoint !=
             FLIP_LIFECYCLE_CHECKPOINT_IMMEDIATE &&
         checkpoint !=
             FLIP_LIFECYCLE_CHECKPOINT_AFTER_FRAME)) {

        return -1;
    }

    memset(
        result,
        0,
        sizeof(*result));

    result->lid_changed =
        actions->lid_changed;

    result->lid_closed =
        actions->lid_closed;

    int observed =
        actions->lid_changed
            ? 1
            : 0;

    if (checkpoint ==
        FLIP_LIFECYCLE_CHECKPOINT_IMMEDIATE) {

        if (actions->lid_changed) {
            fprintf(
                stderr,
                "FLIP_LIFECYCLE: lid %s\n",
                actions->lid_closed
                    ? "closed"
                    : "open");
        }

        if (actions->suspend_requested) {
            observed = 1;

            if (handle_save_to_switcher(
                    FLIP_LIFECYCLE_REASON_SUSPEND,
                    save_fn,
                    userdata,
                    result)) {

                return 1;
            }
        }

        if (actions->gameswitcher_requested) {
            observed = 1;

            if (handle_save_to_switcher(
                    FLIP_LIFECYCLE_REASON_GAMESWITCHER,
                    save_fn,
                    userdata,
                    result)) {

                return 1;
            }
        }

        if (actions->game_options_requested) {
            observed = 1;

            if (handle_save_to_switcher(
                    FLIP_LIFECYCLE_REASON_GAME_OPTIONS,
                    save_fn,
                    userdata,
                    result)) {

                result->exit_code =
                    FLIP_LIFECYCLE_EXIT_GAME_OPTIONS;

                return 1;
            }
        }

        return observed;
    }

    if (!actions->exit_requested)
        return 0;

    result->reason =
        FLIP_LIFECYCLE_REASON_EXIT;

    fprintf(
        stderr,
        "FLIP_LIFECYCLE: exit requested\n");

    run_save(
        save_fn,
        userdata,
        result);

    fprintf(
        stderr,
        "FLIP_LIFECYCLE: exit save %s\n",
        result->save_succeeded
            ? "ok"
            : "failed");

    result->should_exit = 1;
    result->exit_code = 0;

    return 1;
}

int flip_lifecycle_resume_timing(
    flip_timing_t *timing)
{
    if (!timing)
        return -1;

    int rc =
        flip_timing_resync(timing);

    if (rc == 0) {
        fprintf(
            stderr,
            "FLIP_LIFECYCLE: timing resynced\n");
    }

    return rc;
}

int flip_lifecycle_shutdown_shared(void)
{
    int settings_result =
        flip_settings_flush();

    if (settings_result < 0) {
        fprintf(
            stderr,
            "FLIP_LIFECYCLE: settings flush failed\n");
    }

    flip_audio_close();
    flip_input_close();
    flip_video_close();

    return settings_result < 0
        ? -1
        : 0;
}
