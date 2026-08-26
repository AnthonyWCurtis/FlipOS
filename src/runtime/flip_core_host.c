#include "flip_core_host.h"

#include "flip_audio.h"
#include "flip_controls.h"
#include "flip_hotkeys.h"
#include "flip_input.h"
#include "flip_lifecycle.h"
#include "flip_telemetry.h"
#include "flip_timing.h"
#include "flip_video.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    const flip_core_host_config_t *config;
} persistence_bridge_t;

static int lifecycle_save_bridge(
    void *userdata)
{
    persistence_bridge_t *bridge =
        userdata;

    if (!bridge ||
        !bridge->config ||
        !bridge->config->save_state) {

        return -1;
    }

    return bridge->config->save_state(
        bridge->config->core,
        bridge->config->state_userdata);
}

static int cleanup_core(
    flip_core_adapter_t *core,
    int loaded,
    int initialized)
{
    if (loaded)
        flip_core_unload_game(core);

    if (initialized)
        flip_core_deinit(core);

    if (flip_lifecycle_shutdown_shared() < 0) {
        fprintf(
            stderr,
            "FLIP_CORE_HOST: shared cleanup warning\n");
        return -1;
    }

    return 0;
}

static void log_telemetry(
    const flip_telemetry_snapshot_t *snapshot)
{
    if (!snapshot)
        return;

    fprintf(
        stderr,
        "FLIP_TELEMETRY: fps=%.3f cpu=%.1f cpu_mhz=%llu gpu=%.1f gpu_mhz=%llu temp_c=%.1f bat=%d\n",
        snapshot->fps_valid
            ? snapshot->fps
            : -1.0,
        snapshot->cpu_percent_valid
            ? snapshot->cpu_percent
            : -1.0,
        snapshot->cpu_khz_valid
            ? snapshot->cpu_khz / 1000ULL
            : 0ULL,
        snapshot->gpu_percent_valid
            ? snapshot->gpu_percent
            : -1.0,
        snapshot->gpu_hz_valid
            ? snapshot->gpu_hz / 1000000ULL
            : 0ULL,
        snapshot->temp_valid
            ? (double)snapshot->temp_millic / 1000.0
            : -1.0,
        snapshot->battery_valid
            ? snapshot->battery_percent
            : -1);
}

int flip_core_host_run(
    const flip_core_host_config_t *config,
    int *exit_code)
{
    if (!config ||
        !config->core ||
        !config->rom_path ||
        !config->rom_path[0] ||
        !exit_code ||
        flip_core_validate(config->core) < 0) {

        return -1;
    }

    *exit_code = 0;

    if (flip_video_open() < 0) {
        fprintf(
            stderr,
            "FLIP_CORE_HOST: video initialization failed\n");
        return -1;
    }

    if (flip_input_open() < 0) {
        fprintf(
            stderr,
            "FLIP_CORE_HOST: input initialization failed\n");
        flip_video_close();
        return -1;
    }

    int initialized = 0;
    int loaded = 0;
    int host_result = 0;

    if (flip_core_init(config->core) < 0) {
        fprintf(
            stderr,
            "FLIP_CORE_HOST: core initialization failed\n");
        host_result = -1;
        goto cleanup;
    }

    initialized = 1;

    if (flip_core_load_game(
            config->core,
            config->rom_path) < 0) {

        fprintf(
            stderr,
            "FLIP_CORE_HOST: game load failed path=%s\n",
            config->rom_path);
        host_result = -1;
        goto cleanup;
    }

    loaded = 1;

    flip_core_info_t info;
    memset(&info, 0, sizeof(info));

    if (flip_core_get_info(
            config->core,
            &info) < 0) {

        fprintf(
            stderr,
            "FLIP_CORE_HOST: core info failed\n");
        host_result = -1;
        goto cleanup;
    }

    fprintf(
        stderr,
        "FLIP_CORE_HOST: core=%s version=%s video=%ux%u fps=%.3f audio=%u\n",
        info.core_name
            ? info.core_name
            : "?",
        info.core_version
            ? info.core_version
            : "?",
        info.base_width,
        info.base_height,
        info.target_fps,
        info.audio_sample_rate);

    flip_audio_config_t audio_config = {
        .sample_rate = info.audio_sample_rate,
        .channels = 2,
    };

    if (audio_config.sample_rate > 0 &&
        flip_audio_open(&audio_config) < 0) {

        fprintf(
            stderr,
            "FLIP_CORE_HOST: audio initialization failed; continuing silent\n");
    }

    double fps =
        info.target_fps;

    if (fps < 1.0)
        fps = config->fallback_fps;

    if (fps < 1.0) {
        fprintf(
            stderr,
            "FLIP_CORE_HOST: invalid target FPS\n");
        host_result = -1;
        goto cleanup;
    }

    flip_timing_t timing;

    if (flip_timing_init(
            &timing,
            fps) < 0) {

        fprintf(
            stderr,
            "FLIP_CORE_HOST: timing initialization failed\n");
        host_result = -1;
        goto cleanup;
    }

    fprintf(
        stderr,
        "FLIP_CORE_HOST: frame pacing fps=%.6f period_ns=%lld\n",
        fps,
        timing.frame_period_ns);

    flip_telemetry_t telemetry;
    int telemetry_ready =
        flip_telemetry_init(
            &telemetry) == 0;

    if (telemetry_ready) {
        flip_telemetry_sample(
            &telemetry,
            0.0,
            0);
    } else {
        fprintf(
            stderr,
            "FLIP_TELEMETRY: initialization failed\n");
    }

    int restore_result = 0;

    if (config->restore_state) {
        restore_result =
            config->restore_state(
                config->core,
                config->state_userdata);

        if (restore_result < 0) {
            fprintf(
                stderr,
                "FLIP_CORE_HOST: resume unavailable; continuing normally\n");
        }
    }

    if (restore_result > 0) {
        fprintf(
            stderr,
            "FLIP_CORE_HOST: resumed saved session\n");

        if (flip_lifecycle_resume_timing(
                &timing) < 0) {

            fprintf(
                stderr,
                "FLIP_CORE_HOST: resume timing resync failed\n");
        }
    }

    flip_input_state_t physical_input;
    flip_controls_t controls;
    flip_controls_state_t control_state;

    memset(
        &physical_input,
        0,
        sizeof(physical_input));

    memset(
        &control_state,
        0,
        sizeof(control_state));

    flip_controls_init(&controls);

    int perf_overlay_enabled = 0;

    persistence_bridge_t bridge = {
        .config = config,
    };

    while (1) {
        if (flip_input_poll(
                &physical_input,
                0) < 0) {

            fprintf(
                stderr,
                "FLIP_CORE_HOST: input poll failed\n");
            host_result = -1;
            break;
        }

        if (flip_controls_update(
                &controls,
                &physical_input,
                &control_state) < 0) {

            fprintf(
                stderr,
                "FLIP_CORE_HOST: controls update failed\n");
            host_result = -1;
            break;
        }

        if (flip_core_set_input(
                config->core,
                &control_state.game) < 0) {

            fprintf(
                stderr,
                "FLIP_CORE_HOST: core input update failed\n");
            host_result = -1;
            break;
        }

        if (flip_core_run_frame(
                config->core) < 0) {

            fprintf(
                stderr,
                "FLIP_CORE_HOST: core frame failed\n");
            host_result = -1;
            break;
        }

        if (flip_audio_end_frame() < 0) {
            fprintf(
                stderr,
                "FLIP_CORE_HOST: audio frame flush failed\n");
        }

        flip_lifecycle_result_t
            lifecycle_immediate;

        int lifecycle_result =
            flip_lifecycle_handle(
                &control_state.system,
                FLIP_LIFECYCLE_CHECKPOINT_IMMEDIATE,
                lifecycle_save_bridge,
                &bridge,
                &lifecycle_immediate);

        if (lifecycle_result < 0) {
            fprintf(
                stderr,
                "FLIP_CORE_HOST: immediate lifecycle handling failed\n");
            host_result = -1;
            break;
        }

        if (lifecycle_immediate.should_exit) {
            *exit_code =
                lifecycle_immediate.exit_code;
            break;
        }

        if (control_state.system.fps_toggle_requested) {
            perf_overlay_enabled =
                !perf_overlay_enabled;

            flip_video_set_perf_overlay(
                perf_overlay_enabled,
                telemetry_ready
                    ? flip_telemetry_snapshot(
                        &telemetry)
                    : NULL);

            fprintf(
                stderr,
                "FLIP_CORE_HOST: performance overlay %s\n",
                perf_overlay_enabled
                    ? "ON"
                    : "OFF");
        }

        flip_hotkeys_result_t hotkey_result;

        if (flip_hotkeys_apply_actions(
                &control_state.system,
                &hotkey_result) < 0) {

            fprintf(
                stderr,
                "FLIP_CORE_HOST: global hotkey handling failed\n");
        }

        double measured_fps = 0.0;

        int fps_result =
            flip_timing_record_frame(
                &timing,
                &measured_fps);

        if (fps_result < 0) {
            fprintf(
                stderr,
                "FLIP_CORE_HOST: timing FPS measurement failed\n");
            host_result = -1;
            break;
        }

        if (fps_result > 0) {
            if (telemetry_ready) {
                if (flip_telemetry_sample(
                        &telemetry,
                        measured_fps,
                        1) == 0) {

                    const flip_telemetry_snapshot_t *snapshot =
                        flip_telemetry_snapshot(
                            &telemetry);

                    flip_video_set_perf_overlay(
                        perf_overlay_enabled,
                        snapshot);

                    log_telemetry(snapshot);
                }
            }

            fprintf(
                stderr,
                "FLIP_CORE_HOST_FPS %.3f target=%.3f\n",
                measured_fps,
                fps);
        }

        flip_lifecycle_result_t
            lifecycle_after_frame;

        lifecycle_result =
            flip_lifecycle_handle(
                &control_state.system,
                FLIP_LIFECYCLE_CHECKPOINT_AFTER_FRAME,
                lifecycle_save_bridge,
                &bridge,
                &lifecycle_after_frame);

        if (lifecycle_result < 0) {
            fprintf(
                stderr,
                "FLIP_CORE_HOST: after-frame lifecycle handling failed\n");
            host_result = -1;
            break;
        }

        if (lifecycle_after_frame.should_exit) {
            *exit_code =
                lifecycle_after_frame.exit_code;
            break;
        }

        int wait_result =
            flip_timing_wait_frame(
                &timing);

        if (wait_result < 0) {
            fprintf(
                stderr,
                "FLIP_CORE_HOST: timing wait failed\n");
            host_result = -1;
            break;
        }

        if (wait_result > 0) {
            fprintf(
                stderr,
                "FLIP_CORE_HOST: frame schedule resync\n");
        }
    }

cleanup:
    if (cleanup_core(
            config->core,
            loaded,
            initialized) < 0 &&
        host_result == 0) {

        host_result = -1;
    }

    return host_result;
}
