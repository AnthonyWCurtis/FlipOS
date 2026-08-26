#include "flip_lifecycle.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static int save_results[8];
static int save_result_count;
static int save_calls;

static char cleanup_order[16];
static int cleanup_count;
static int settings_flush_result;

static int save_cb(void *userdata)
{
    (void)userdata;

    int index =
        save_calls++;

    if (index >= save_result_count)
        return -1;

    return save_results[index];
}

int flip_settings_flush(void)
{
    cleanup_order[cleanup_count++] = 'S';
    cleanup_order[cleanup_count] = '\0';

    return settings_flush_result;
}

void flip_audio_close(void)
{
    cleanup_order[cleanup_count++] = 'A';
    cleanup_order[cleanup_count] = '\0';
}

void flip_input_close(void)
{
    cleanup_order[cleanup_count++] = 'I';
    cleanup_order[cleanup_count] = '\0';
}

void flip_video_close(void)
{
    cleanup_order[cleanup_count++] = 'V';
    cleanup_order[cleanup_count] = '\0';
}

static void reset_save(
    int first,
    int second,
    int count)
{
    memset(
        save_results,
        0,
        sizeof(save_results));

    save_results[0] = first;
    save_results[1] = second;
    save_result_count = count;
    save_calls = 0;
}

static void test_no_action(void)
{
    flip_system_actions_t actions = {0};
    flip_lifecycle_result_t result;

    reset_save(0, 0, 0);

    assert(flip_lifecycle_handle(
        &actions,
        FLIP_LIFECYCLE_CHECKPOINT_IMMEDIATE,
        save_cb,
        NULL,
        &result) == 0);

    assert(result.should_exit == 0);
    assert(result.reason ==
        FLIP_LIFECYCLE_REASON_NONE);
    assert(save_calls == 0);
}

static void test_lid_transition(void)
{
    flip_system_actions_t actions = {0};
    flip_lifecycle_result_t result;

    actions.lid_changed = 1;
    actions.lid_closed = 1;

    assert(flip_lifecycle_handle(
        &actions,
        FLIP_LIFECYCLE_CHECKPOINT_IMMEDIATE,
        save_cb,
        NULL,
        &result) == 1);

    assert(result.lid_changed == 1);
    assert(result.lid_closed == 1);
    assert(result.should_exit == 0);
}

static void test_suspend_success(void)
{
    flip_system_actions_t actions = {0};
    flip_lifecycle_result_t result;

    actions.suspend_requested = 1;
    reset_save(0, 0, 1);

    assert(flip_lifecycle_handle(
        &actions,
        FLIP_LIFECYCLE_CHECKPOINT_IMMEDIATE,
        save_cb,
        NULL,
        &result) == 1);

    assert(save_calls == 1);
    assert(result.reason ==
        FLIP_LIFECYCLE_REASON_SUSPEND);
    assert(result.save_attempted == 1);
    assert(result.save_succeeded == 1);
    assert(result.should_exit == 1);
    assert(result.exit_code ==
        FLIP_LIFECYCLE_EXIT_SWITCHER);
}

static void test_suspend_failure(void)
{
    flip_system_actions_t actions = {0};
    flip_lifecycle_result_t result;

    actions.suspend_requested = 1;
    reset_save(-1, 0, 1);

    assert(flip_lifecycle_handle(
        &actions,
        FLIP_LIFECYCLE_CHECKPOINT_IMMEDIATE,
        save_cb,
        NULL,
        &result) == 1);

    assert(save_calls == 1);
    assert(result.reason ==
        FLIP_LIFECYCLE_REASON_SUSPEND);
    assert(result.save_succeeded == 0);
    assert(result.should_exit == 0);
}

static void test_failed_suspend_then_switcher(void)
{
    flip_system_actions_t actions = {0};
    flip_lifecycle_result_t result;

    actions.suspend_requested = 1;
    actions.gameswitcher_requested = 1;
    reset_save(-1, 0, 2);

    assert(flip_lifecycle_handle(
        &actions,
        FLIP_LIFECYCLE_CHECKPOINT_IMMEDIATE,
        save_cb,
        NULL,
        &result) == 1);

    assert(save_calls == 2);
    assert(result.reason ==
        FLIP_LIFECYCLE_REASON_GAMESWITCHER);
    assert(result.save_succeeded == 1);
    assert(result.should_exit == 1);
    assert(result.exit_code ==
        FLIP_LIFECYCLE_EXIT_SWITCHER);
}

static void test_switcher_success(void)
{
    flip_system_actions_t actions = {0};
    flip_lifecycle_result_t result;

    actions.gameswitcher_requested = 1;
    reset_save(0, 0, 1);

    assert(flip_lifecycle_handle(
        &actions,
        FLIP_LIFECYCLE_CHECKPOINT_IMMEDIATE,
        save_cb,
        NULL,
        &result) == 1);

    assert(save_calls == 1);
    assert(result.reason ==
        FLIP_LIFECYCLE_REASON_GAMESWITCHER);
    assert(result.should_exit == 1);
    assert(result.exit_code ==
        FLIP_LIFECYCLE_EXIT_SWITCHER);
}

static void test_immediate_ignores_exit(void)
{
    flip_system_actions_t actions = {0};
    flip_lifecycle_result_t result;

    actions.exit_requested = 1;
    reset_save(0, 0, 1);

    assert(flip_lifecycle_handle(
        &actions,
        FLIP_LIFECYCLE_CHECKPOINT_IMMEDIATE,
        save_cb,
        NULL,
        &result) == 0);

    assert(save_calls == 0);
    assert(result.should_exit == 0);
}

static void test_after_frame_exit_success(void)
{
    flip_system_actions_t actions = {0};
    flip_lifecycle_result_t result;

    actions.exit_requested = 1;
    reset_save(0, 0, 1);

    assert(flip_lifecycle_handle(
        &actions,
        FLIP_LIFECYCLE_CHECKPOINT_AFTER_FRAME,
        save_cb,
        NULL,
        &result) == 1);

    assert(save_calls == 1);
    assert(result.reason ==
        FLIP_LIFECYCLE_REASON_EXIT);
    assert(result.save_succeeded == 1);
    assert(result.should_exit == 1);
    assert(result.exit_code == 0);
}

static void test_after_frame_exit_failure_still_exits(void)
{
    flip_system_actions_t actions = {0};
    flip_lifecycle_result_t result;

    actions.exit_requested = 1;
    reset_save(-1, 0, 1);

    assert(flip_lifecycle_handle(
        &actions,
        FLIP_LIFECYCLE_CHECKPOINT_AFTER_FRAME,
        save_cb,
        NULL,
        &result) == 1);

    assert(save_calls == 1);
    assert(result.save_succeeded == 0);
    assert(result.should_exit == 1);
    assert(result.exit_code == 0);
}

static void test_after_frame_ignores_immediate_actions(void)
{
    flip_system_actions_t actions = {0};
    flip_lifecycle_result_t result;

    actions.suspend_requested = 1;
    actions.gameswitcher_requested = 1;
    reset_save(0, 0, 2);

    assert(flip_lifecycle_handle(
        &actions,
        FLIP_LIFECYCLE_CHECKPOINT_AFTER_FRAME,
        save_cb,
        NULL,
        &result) == 0);

    assert(save_calls == 0);
    assert(result.should_exit == 0);
}

static void test_resume_timing(void)
{
    flip_timing_t timing;

    assert(flip_timing_init(
        &timing,
        59.7275) == 0);

    timing.fps_frames = 99;
    timing.measured_fps = 12.5;

    assert(flip_lifecycle_resume_timing(
        &timing) == 0);

    assert(timing.fps_frames == 0);
    assert(timing.measured_fps == 0.0);
}

static void test_cleanup_order(void)
{
    memset(
        cleanup_order,
        0,
        sizeof(cleanup_order));

    cleanup_count = 0;
    settings_flush_result = 0;

    assert(flip_lifecycle_shutdown_shared() == 0);
    assert(strcmp(
        cleanup_order,
        "SAIV") == 0);
}

static void test_cleanup_continues_after_settings_failure(void)
{
    memset(
        cleanup_order,
        0,
        sizeof(cleanup_order));

    cleanup_count = 0;
    settings_flush_result = -1;

    assert(flip_lifecycle_shutdown_shared() == -1);
    assert(strcmp(
        cleanup_order,
        "SAIV") == 0);
}

int main(void)
{
    test_no_action();
    test_lid_transition();
    test_suspend_success();
    test_suspend_failure();
    test_failed_suspend_then_switcher();
    test_switcher_success();
    test_immediate_ignores_exit();
    test_after_frame_exit_success();
    test_after_frame_exit_failure_still_exits();
    test_after_frame_ignores_immediate_actions();
    test_resume_timing();
    test_cleanup_order();
    test_cleanup_continues_after_settings_failure();

    puts("flip_lifecycle tests passed");
    return 0;
}
