#include "flip_core_host.h"

#include "flip_audio.h"
#include "flip_hotkeys.h"
#include "flip_lifecycle.h"
#include "flip_video.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char *events[64];
static int event_count;
static int restore_result;
static int resume_calls;

static void event(const char *name)
{
    assert(event_count < (int)(sizeof(events) / sizeof(events[0])));
    events[event_count++] = name;
}

static int find_event(const char *name)
{
    for (int i = 0; i < event_count; ++i) {
        if (strcmp(events[i], name) == 0)
            return i;
    }

    return -1;
}

typedef struct {
    flip_game_input_t input;
} fake_core_t;

static int fake_init(void *userdata)
{
    (void)userdata;
    event("core_init");
    return 0;
}

static void fake_deinit(void *userdata)
{
    (void)userdata;
    event("core_deinit");
}

static int fake_load(void *userdata, const char *path)
{
    (void)userdata;
    assert(strcmp(path, "/roms/test.gb") == 0);
    event("core_load");
    return 0;
}

static void fake_unload(void *userdata)
{
    (void)userdata;
    event("core_unload");
}

static int fake_info(void *userdata, flip_core_info_t *info)
{
    (void)userdata;
    event("core_info");

    memset(info, 0, sizeof(*info));
    info->core_name = "fake";
    info->core_version = "1";
    info->target_fps = 59.75;
    info->audio_sample_rate = 32768;
    info->base_width = 160;
    info->base_height = 144;
    return 0;
}

static int fake_set_input(void *userdata, const flip_game_input_t *input)
{
    fake_core_t *fake = userdata;
    fake->input = *input;
    event("core_input");
    return 0;
}

static int fake_run(void *userdata)
{
    fake_core_t *fake = userdata;
    assert(fake->input.down[FLIP_GAME_A] == 1);
    event("core_frame");
    return 0;
}

static size_t fake_serialize_size(void *userdata)
{
    (void)userdata;
    return 8;
}

static int fake_serialize(void *userdata, void *data, size_t size)
{
    (void)userdata;
    (void)data;
    return size == 8 ? 0 : -1;
}

static int fake_unserialize(void *userdata, const void *data, size_t size)
{
    (void)userdata;
    (void)data;
    return size == 8 ? 0 : -1;
}

static flip_core_adapter_t make_core(fake_core_t *fake)
{
    flip_core_adapter_t core = {
        .userdata = fake,
        .init = fake_init,
        .deinit = fake_deinit,
        .load_game = fake_load,
        .unload_game = fake_unload,
        .get_info = fake_info,
        .set_input = fake_set_input,
        .run_frame = fake_run,
        .serialize_size = fake_serialize_size,
        .serialize = fake_serialize,
        .unserialize = fake_unserialize,
    };

    return core;
}

static int save_state(
    flip_core_adapter_t *core,
    void *userdata)
{
    (void)core;
    (void)userdata;
    event("save_state");
    return 0;
}

static int restore_state(
    flip_core_adapter_t *core,
    void *userdata)
{
    (void)core;
    (void)userdata;
    event("restore");
    return restore_result;
}

static int load_save(
    flip_core_adapter_t *core,
    void *userdata)
{
    (void)core;
    (void)userdata;
    event("load_save");
    return 0;
}

static int commit_save(
    flip_core_adapter_t *core,
    void *userdata)
{
    (void)core;
    (void)userdata;
    event("commit_save");
    return 0;
}

/* Production-host dependencies are replaced by link-time test stubs. */
int flip_video_open(void)
{
    event("video_open");
    return 0;
}

void flip_video_close(void)
{
    event("video_close");
}

void flip_video_set_fps_overlay(int enabled, double fps)
{
    (void)enabled;
    (void)fps;
}

int flip_video_submit(
    const void *data,
    unsigned width,
    unsigned height,
    size_t pitch,
    flip_video_format_t format)
{
    (void)data;
    (void)width;
    (void)height;
    (void)pitch;
    (void)format;
    return 0;
}

int flip_input_open(void)
{
    event("input_open");
    return 0;
}

void flip_input_close(void)
{
    event("input_close");
}

int flip_input_poll(flip_input_state_t *state, int timeout_ms)
{
    (void)timeout_ms;
    memset(state, 0, sizeof(*state));
    event("poll");
    return 0;
}

const char *flip_button_name(flip_button_t button)
{
    (void)button;
    return "test";
}

void flip_controls_init(flip_controls_t *controls)
{
    memset(controls, 0, sizeof(*controls));
}

int flip_controls_update(
    flip_controls_t *controls,
    const flip_input_state_t *input,
    flip_controls_state_t *state)
{
    (void)controls;
    (void)input;

    memset(state, 0, sizeof(*state));
    state->game.down[FLIP_GAME_A] = 1;
    state->system.exit_requested = 1;
    event("controls");
    return 0;
}

int flip_audio_open(const flip_audio_config_t *config)
{
    assert(config != NULL);
    assert(config->source_sample_rate == 32768);
    assert(config->output_sample_rate == 32000);
    assert(config->channels == 2);
    event("audio_open");
    return 0;
}

size_t flip_audio_submit(const int16_t *samples, size_t frames)
{
    (void)samples;
    return frames;
}

int flip_audio_end_frame(void)
{
    event("audio_end");
    return 0;
}

void flip_audio_diag_snapshot(flip_audio_diag_snapshot_t *snapshot)
{
    if (!snapshot)
        return;

    memset(snapshot, 0, sizeof(*snapshot));
}

void flip_audio_diag_reset(void)
{
}

void flip_audio_close(void)
{
    event("audio_close");
}

int flip_audio_set_volume(int percent)
{
    return percent;
}

int flip_audio_get_volume(void)
{
    return 50;
}

int flip_hotkeys_apply_actions(
    const flip_system_actions_t *actions,
    flip_hotkeys_result_t *result)
{
    (void)actions;
    memset(result, 0, sizeof(*result));
    event("hotkeys");
    return 0;
}

void flip_hotkeys_init(flip_hotkeys_t *hotkeys)
{
    memset(hotkeys, 0, sizeof(*hotkeys));
}

int flip_hotkeys_handle(
    flip_hotkeys_t *hotkeys,
    const flip_input_state_t *input,
    flip_hotkeys_result_t *result)
{
    (void)hotkeys;
    (void)input;
    (void)result;
    return 0;
}

int flip_timing_init(flip_timing_t *timing, double target_fps)
{
    memset(timing, 0, sizeof(*timing));
    timing->target_fps = target_fps;
    event("timing_init");
    return 0;
}

int flip_timing_record_frame(flip_timing_t *timing, double *measured_fps)
{
    (void)timing;
    *measured_fps = 59.75;
    event("timing_record");
    return 0;
}

int flip_timing_wait_frame(flip_timing_t *timing)
{
    (void)timing;
    event("timing_wait");
    return 0;
}

int flip_timing_resync(flip_timing_t *timing)
{
    (void)timing;
    return 0;
}

int flip_lifecycle_handle(
    const flip_system_actions_t *actions,
    flip_lifecycle_checkpoint_t checkpoint,
    flip_lifecycle_save_fn save_fn,
    void *userdata,
    flip_lifecycle_result_t *result)
{
    memset(result, 0, sizeof(*result));

    if (checkpoint == FLIP_LIFECYCLE_CHECKPOINT_IMMEDIATE) {
        event("life_immediate");
        return 0;
    }

    event("life_after");

    if (actions->exit_requested) {
        if (save_fn)
            assert(save_fn(userdata) == 0);

        result->reason = FLIP_LIFECYCLE_REASON_EXIT;
        result->should_exit = 1;
        result->exit_code = 0;
        return 1;
    }

    return 0;
}

int flip_lifecycle_resume_timing(flip_timing_t *timing)
{
    (void)timing;
    resume_calls++;
    event("resume");
    return 0;
}

int flip_lifecycle_shutdown_shared(void)
{
    event("shutdown");
    return 0;
}

static void reset_test(void)
{
    memset(events, 0, sizeof(events));
    event_count = 0;
    restore_result = 0;
    resume_calls = 0;
}

static void assert_before(const char *first, const char *second)
{
    int a = find_event(first);
    int b = find_event(second);

    assert(a >= 0);
    assert(b >= 0);
    assert(a < b);
}

static flip_core_host_config_t make_config(flip_core_adapter_t *core)
{
    flip_core_host_config_t config = {
        .core = core,
        .rom_path = "/roms/test.gb",
        .fallback_fps = 59.7275,
        .save_state = save_state,
        .restore_state = restore_state,
        .state_userdata = NULL,
        .load_save = load_save,
        .commit_save = commit_save,
        .save_userdata = NULL,
    };

    return config;
}

static void test_host_owns_frame_order_and_exit_code(void)
{
    reset_test();

    fake_core_t fake;
    memset(&fake, 0, sizeof(fake));
    flip_core_adapter_t core = make_core(&fake);
    flip_core_host_config_t config = make_config(&core);

    int exit_code = -1;
    assert(flip_core_host_run(&config, &exit_code) == 0);
    assert(exit_code == 0);

    assert_before("core_load", "load_save");
    assert_before("load_save", "restore");
    assert_before("restore", "poll");
    assert_before("poll", "controls");
    assert_before("controls", "core_input");
    assert_before("core_input", "core_frame");
    assert_before("core_frame", "audio_end");
    assert_before("audio_end", "life_immediate");
    assert_before("life_immediate", "hotkeys");
    assert_before("hotkeys", "timing_record");
    assert_before("timing_record", "life_after");
    assert_before("life_after", "commit_save");
    assert_before("commit_save", "save_state");
    assert_before("save_state", "core_unload");
    assert_before("core_unload", "core_deinit");
    assert_before("core_deinit", "shutdown");
}

static void test_resume_resync_only_after_actual_restore(void)
{
    reset_test();

    fake_core_t fake;
    memset(&fake, 0, sizeof(fake));
    flip_core_adapter_t core = make_core(&fake);
    flip_core_host_config_t config = make_config(&core);

    int exit_code;

    restore_result = 0;
    assert(flip_core_host_run(&config, &exit_code) == 0);
    assert(resume_calls == 0);

    reset_test();
    memset(&fake, 0, sizeof(fake));
    core = make_core(&fake);
    config = make_config(&core);
    restore_result = 1;

    assert(flip_core_host_run(&config, &exit_code) == 0);
    assert(resume_calls == 1);
    assert_before("load_save", "restore");
    assert_before("restore", "resume");
    assert_before("resume", "poll");
}

int main(void)
{
    test_host_owns_frame_order_and_exit_code();
    test_resume_resync_only_after_actual_restore();

    puts("flip_core_host tests passed");
    return 0;
}
