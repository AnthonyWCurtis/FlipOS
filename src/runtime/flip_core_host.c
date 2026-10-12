#define _POSIX_C_SOURCE 200809L

#include "flip_core_host.h"

#include "flip_audio.h"
#include "flip_battery.h"
#include "flip_controls.h"
#include "flip_cpu_dynamic.h"
#include "flip_hotkeys.h"
#include "flip_input.h"
#include "flip_lifecycle.h"
#include "flip_log.h"
#include "flip_osd.h"
#include "flip_power_policy.h"
#include "flip_perf_diag.h"
#include "flip_settings.h"
#include "flip_telemetry.h"
#include "flip_timing.h"
#include "flip_video.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    const flip_core_host_config_t *config;
} persistence_bridge_t;

static int lifecycle_save_bridge(void *userdata)
{
    persistence_bridge_t *bridge = userdata;

    if (!bridge || !bridge->config || !bridge->config->save_state)
        return -1;

    if (bridge->config->commit_save &&
        bridge->config->commit_save(
            bridge->config->core,
            bridge->config->save_userdata) < 0) {
        return -1;
    }

    if (bridge->config->save_state(
            bridge->config->core,
            bridge->config->state_userdata) < 0) {
        return -1;
    }

    if (bridge->config->persistence_complete &&
        bridge->config->persistence_complete(
            bridge->config->persistence_userdata) < 0) {
        return -1;
    }

    return 0;
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
        fprintf(stderr, "FLIP_CORE_HOST: shared cleanup warning\n");
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
        snapshot->fps_valid ? snapshot->fps : -1.0,
        snapshot->cpu_percent_valid ? snapshot->cpu_percent : -1.0,
        snapshot->cpu_khz_valid ? snapshot->cpu_khz / 1000ULL : 0ULL,
        snapshot->gpu_percent_valid ? snapshot->gpu_percent : -1.0,
        snapshot->gpu_hz_valid ? snapshot->gpu_hz / 1000000ULL : 0ULL,
        snapshot->temp_valid ? (double)snapshot->temp_millic / 1000.0 : -1.0,
        snapshot->battery_valid ? snapshot->battery_percent : -1);
}

static uint64_t monotonic_us(void)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0)
        return 0;
    return (uint64_t)now.tv_sec * 1000000ULL + (uint64_t)now.tv_nsec / 1000ULL;
}

static uint64_t elapsed_us(uint64_t start_us)
{
    uint64_t now_us = monotonic_us();
    if (now_us < start_us)
        return 0;
    return now_us - start_us;
}

static flip_audio_diag_snapshot_t log_audio_diag(void)
{
    flip_audio_diag_snapshot_t audio_diag;
    memset(&audio_diag, 0, sizeof(audio_diag));
    flip_audio_diag_snapshot(&audio_diag);

    uint64_t submit_avg_us = audio_diag.submit_count > 0
        ? audio_diag.submit_total_us / audio_diag.submit_count : 0;
    uint64_t write_avg_us = audio_diag.transport_write_count > 0
        ? audio_diag.transport_write_total_us / audio_diag.transport_write_count : 0;

    fprintf(stderr,
        "FLIP_AUDIO_PERF: submits=%llu submit_avg_us=%llu submit_max_us=%llu "
        "writes=%llu write_avg_us=%llu write_max_us=%llu write_total_us=%llu "
        "queue_peak_frames=%llu queue_dropped_frames=%llu\n",
        (unsigned long long)audio_diag.submit_count,
        (unsigned long long)submit_avg_us,
        (unsigned long long)audio_diag.submit_max_us,
        (unsigned long long)audio_diag.transport_write_count,
        (unsigned long long)write_avg_us,
        (unsigned long long)audio_diag.transport_write_max_us,
        (unsigned long long)audio_diag.transport_write_total_us,
        (unsigned long long)audio_diag.queue_peak_frames,
        (unsigned long long)audio_diag.queue_dropped_frames);

    flip_audio_diag_reset();
    return audio_diag;
}

static void log_perf_diag(const flip_perf_diag_t *diag, double measured_fps)
{
    if (!diag || diag->frames == 0)
        return;

    fprintf(stderr,
        "FLIP_PERF: fps=%.3f frames=%llu input_avg_us=%llu input_max_us=%llu controls_avg_us=%llu controls_max_us=%llu core_input_avg_us=%llu core_input_max_us=%llu core_run_avg_us=%llu core_run_max_us=%llu audio_avg_us=%llu audio_max_us=%llu host_avg_us=%llu host_max_us=%llu late_avg_us=%llu late_max_us=%llu late_frames=%llu resyncs=%llu\n",
        measured_fps,
        (unsigned long long)diag->frames,
        (unsigned long long)flip_perf_metric_avg_us(&diag->input, diag->frames),
        (unsigned long long)diag->input.max_us,
        (unsigned long long)flip_perf_metric_avg_us(&diag->controls, diag->frames),
        (unsigned long long)diag->controls.max_us,
        (unsigned long long)flip_perf_metric_avg_us(&diag->core_input, diag->frames),
        (unsigned long long)diag->core_input.max_us,
        (unsigned long long)flip_perf_metric_avg_us(&diag->core_run, diag->frames),
        (unsigned long long)diag->core_run.max_us,
        (unsigned long long)flip_perf_metric_avg_us(&diag->audio, diag->frames),
        (unsigned long long)diag->audio.max_us,
        (unsigned long long)flip_perf_metric_avg_us(&diag->host, diag->frames),
        (unsigned long long)diag->host.max_us,
        (unsigned long long)flip_perf_metric_avg_us(&diag->late, diag->late_frames),
        (unsigned long long)diag->late.max_us,
        (unsigned long long)diag->late_frames,
        (unsigned long long)diag->resyncs);
}

static unsigned dynamic_cpu_allowed_max_from_env(void)
{
    const char *text =
        getenv("FLIP_CPU_DYNAMIC_MAX_KHZ");

    if (!text || !text[0])
        return 0;

    char *end = NULL;
    unsigned long value =
        strtoul(text, &end, 10);

    if (end == text ||
        *end != '\0' ||
        value == 0 ||
        value > 0xffffffffUL)
        return 0;

    return (unsigned)value;
}

int flip_core_host_run(const flip_core_host_config_t *config, int *exit_code)
{
    if (!config || !config->core || !config->rom_path || !config->rom_path[0] ||
        !exit_code || flip_core_validate(config->core) < 0)
        return -1;

    *exit_code = 0;

    flip_osd_init();

    if (flip_settings_init() < 0) {
        fprintf(stderr, "FLIP_CORE_HOST: settings initialization failed\n");
    }

    if (flip_video_open() < 0) {
        fprintf(stderr, "FLIP_CORE_HOST: video initialization failed\n");
        return -1;
    }
    if (flip_input_open() < 0) {
        fprintf(stderr, "FLIP_CORE_HOST: input initialization failed\n");
        flip_video_close();
        return -1;
    }

    int initialized = 0;
    int loaded = 0;
    int host_result = 0;

    if (flip_core_init(config->core) < 0) {
        fprintf(stderr, "FLIP_CORE_HOST: core initialization failed\n");
        host_result = -1;
        goto cleanup;
    }
    initialized = 1;

    if (flip_core_load_game(config->core, config->rom_path) < 0) {
        fprintf(stderr, "FLIP_CORE_HOST: game load failed path=%s\n", config->rom_path);
        host_result = -1;
        goto cleanup;
    }
    loaded = 1;

    size_t battery_memory_size = flip_core_save_memory_size(config->core);
    fprintf(stderr, "FLIP_SAVE: battery_memory size=%zu\n", battery_memory_size);

    if (config->load_save && config->load_save(config->core, config->save_userdata) < 0) {
        fprintf(stderr, "FLIP_CORE_HOST: cartridge save load failed\n");
        host_result = -1;
        goto cleanup;
    }

    flip_core_info_t info;
    memset(&info, 0, sizeof(info));
    if (flip_core_get_info(config->core, &info) < 0) {
        fprintf(stderr, "FLIP_CORE_HOST: core info failed\n");
        host_result = -1;
        goto cleanup;
    }

    fprintf(stderr, "FLIP_CORE_HOST: core=%s version=%s video=%ux%u fps=%.3f audio=%u\n",
        info.core_name ? info.core_name : "?",
        info.core_version ? info.core_version : "?",
        info.base_width, info.base_height, info.target_fps, info.audio_sample_rate);

    flip_audio_config_t audio_config = {
        .source_sample_rate = info.audio_sample_rate,
        .output_sample_rate = 32000,
        .channels = 2,
    };
    if (audio_config.source_sample_rate > 0 && flip_audio_open(&audio_config) < 0)
        fprintf(stderr, "FLIP_CORE_HOST: audio initialization failed; continuing silent\n");

    double fps = info.target_fps;
    if (fps < 1.0)
        fps = config->fallback_fps;
    if (fps < 1.0) {
        fprintf(stderr, "FLIP_CORE_HOST: invalid target FPS\n");
        host_result = -1;
        goto cleanup;
    }

    flip_timing_t timing;
    if (flip_timing_init(&timing, fps) < 0) {
        fprintf(stderr, "FLIP_CORE_HOST: timing initialization failed\n");
        host_result = -1;
        goto cleanup;
    }
    fprintf(stderr, "FLIP_CORE_HOST: frame pacing fps=%.6f period_ns=%lld\n", fps, timing.frame_period_ns);

    flip_telemetry_t telemetry;
    int telemetry_ready = flip_telemetry_init(&telemetry) == 0;
    if (!telemetry_ready)
        fprintf(stderr, "FLIP_TELEMETRY: initialization failed\n");

    flip_perf_diag_t perf_diag;
    flip_perf_diag_reset(&perf_diag);
    flip_audio_diag_reset();

    flip_cpu_dynamic_t dynamic_cpu;
    memset(&dynamic_cpu, 0, sizeof(dynamic_cpu));

    unsigned dynamic_cpu_allowed_max =
        dynamic_cpu_allowed_max_from_env();

    if (dynamic_cpu_allowed_max > 0) {
        int dynamic_result =
            flip_cpu_dynamic_init(
                &dynamic_cpu,
                dynamic_cpu_allowed_max);

        if (dynamic_result > 0) {
            fprintf(
                stderr,
                "FLIP_CPU_DYNAMIC: enabled current_max_khz=%u allowed_max_khz=%u warmup=%u\n",
                dynamic_cpu.current_max_khz,
                dynamic_cpu.allowed_max_khz,
                dynamic_cpu.warmup_windows);
        } else if (dynamic_result == 0) {
            fprintf(
                stderr,
                "FLIP_CPU_DYNAMIC: no escalation range current_max_khz=%u allowed_max_khz=%u\n",
                dynamic_cpu.current_max_khz,
                dynamic_cpu.allowed_max_khz);
        } else {
            fprintf(
                stderr,
                "FLIP_CPU_DYNAMIC: initialization failed; leaving policy unchanged\n");
        }
    }

    int restore_result = 0;
    if (config->restore_state) {
        restore_result = config->restore_state(config->core, config->state_userdata);
        if (restore_result < 0)
            fprintf(stderr, "FLIP_CORE_HOST: resume unavailable; continuing normally\n");
    }
    if (restore_result > 0) {
        fprintf(stderr, "FLIP_CORE_HOST: resumed saved session\n");

        if (config->post_restore &&
            config->post_restore(
                config->core,
                config->post_restore_userdata) < 0) {
            fprintf(
                stderr,
                "FLIP_CORE_HOST: post-resume reconciliation failed\n");
        } else if (config->post_restore) {
            fprintf(
                stderr,
                "FLIP_CORE_HOST: post-resume reconciliation complete\n");
        }

        if (flip_lifecycle_resume_timing(&timing) < 0)
            fprintf(stderr, "FLIP_CORE_HOST: resume timing resync failed\n");
    }

    flip_input_state_t physical_input;
    flip_controls_t controls;
    flip_controls_state_t control_state;
    memset(&physical_input, 0, sizeof(physical_input));
    memset(&control_state, 0, sizeof(control_state));
    flip_controls_init(&controls);

    int perf_overlay_enabled = 0;
    double last_measured_fps = 0.0;
    int last_measured_fps_valid = 0;
    persistence_bridge_t bridge = { .config = config };

    flip_low_battery_guard_t low_battery_guard;
    flip_low_battery_guard_init(&low_battery_guard);
    uint64_t next_battery_check_us = monotonic_us();
    int low_battery_warning_visible = 0;

    while (1) {
        uint64_t frame_start_us = monotonic_us();
        uint64_t stage_start_us = frame_start_us;

        int input_result = flip_input_poll(&physical_input, 0);
        uint64_t input_us = elapsed_us(stage_start_us);
        if (input_result < 0) {
            fprintf(stderr, "FLIP_CORE_HOST: input poll failed\n");
            host_result = -1;
            break;
        }

        uint64_t battery_now_us = monotonic_us();
        if (battery_now_us >= next_battery_check_us) {
            flip_battery_snapshot_t battery;
            next_battery_check_us = battery_now_us + 1000000ULL;

            if (flip_battery_read(&battery) == 0) {
                flip_low_battery_action_t battery_action =
                    flip_low_battery_guard_update(
                        &low_battery_guard,
                        &battery,
                        1000U);

                if (battery_action == FLIP_LOW_BATTERY_WARN) {
                    if (!low_battery_warning_visible) {
                        flip_osd_show_status(
                            "LOW BATTERY - CONNECT CHARGER");
                        low_battery_warning_visible = 1;
                    }
                } else if (battery_action == FLIP_LOW_BATTERY_OK) {
                    low_battery_warning_visible = 0;
                }

                if (battery_action == FLIP_LOW_BATTERY_SHUTDOWN) {
                    flip_log_write(
                        FLIP_LOG_WARN,
                        "FLIP_POWER",
                        "critical battery percent=%d voltage_uv=%ld external=%d; saving before shutdown",
                        battery.percent_valid ? battery.percent : -1,
                        battery.voltage_valid ? battery.voltage_uv : -1L,
                        battery.external_power_valid
                            ? battery.external_power_online
                            : -1);

                    flip_osd_show_status(
                        "BATTERY CRITICAL - SAVING");

                    int save_result =
                        lifecycle_save_bridge(&bridge);

                    flip_log_write(
                        save_result == 0
                            ? FLIP_LOG_INFO
                            : FLIP_LOG_ERROR,
                        "FLIP_POWER",
                        "critical battery save %s",
                        save_result == 0 ? "ok" : "failed");

                    *exit_code = FLIP_EXIT_CODE_LOW_BATTERY;
                    break;
                }
            }
        }

        stage_start_us = monotonic_us();
        int controls_result = flip_controls_update(&controls, &physical_input, &control_state);
        uint64_t controls_us = elapsed_us(stage_start_us);
        if (controls_result < 0) {
            fprintf(stderr, "FLIP_CORE_HOST: controls update failed\n");
            host_result = -1;
            break;
        }

        stage_start_us = monotonic_us();
        int core_input_result = flip_core_set_input(config->core, &control_state.game);
        uint64_t core_input_us = elapsed_us(stage_start_us);
        if (core_input_result < 0) {
            fprintf(stderr, "FLIP_CORE_HOST: core input update failed\n");
            host_result = -1;
            break;
        }

        stage_start_us = monotonic_us();
        int core_run_result = flip_core_run_frame(config->core);
        uint64_t core_run_us = elapsed_us(stage_start_us);
        if (core_run_result < 0) {
            fprintf(stderr, "FLIP_CORE_HOST: core frame failed\n");
            host_result = -1;
            break;
        }

        stage_start_us = monotonic_us();
        int audio_result = flip_audio_end_frame();
        uint64_t audio_us = elapsed_us(stage_start_us);
        if (audio_result < 0)
            fprintf(stderr, "FLIP_CORE_HOST: audio frame flush failed\n");

        flip_lifecycle_result_t lifecycle_immediate;
        int lifecycle_result = flip_lifecycle_handle(
            &control_state.system,
            FLIP_LIFECYCLE_CHECKPOINT_IMMEDIATE,
            lifecycle_save_bridge,
            &bridge,
            &lifecycle_immediate);
        if (lifecycle_result < 0) {
            fprintf(stderr, "FLIP_CORE_HOST: immediate lifecycle handling failed\n");
            host_result = -1;
            break;
        }
        if (lifecycle_immediate.should_exit) {
            *exit_code = lifecycle_immediate.exit_code;
            break;
        }

        if (control_state.system.fps_toggle_requested) {
            perf_overlay_enabled = !perf_overlay_enabled;
            if (perf_overlay_enabled && telemetry_ready)
                flip_telemetry_sample(&telemetry, last_measured_fps, last_measured_fps_valid);
            flip_video_set_perf_overlay(perf_overlay_enabled,
                telemetry_ready ? flip_telemetry_snapshot(&telemetry) : NULL);
            fprintf(stderr, "FLIP_CORE_HOST: performance overlay %s\n",
                perf_overlay_enabled ? "ON" : "OFF");
        }

        if (flip_hotkeys_apply_actions(&control_state.system) < 0)
            fprintf(stderr, "FLIP_CORE_HOST: global hotkey handling failed\n");

        flip_lifecycle_result_t lifecycle_after_frame;
        lifecycle_result = flip_lifecycle_handle(
            &control_state.system,
            FLIP_LIFECYCLE_CHECKPOINT_AFTER_FRAME,
            lifecycle_save_bridge,
            &bridge,
            &lifecycle_after_frame);
        if (lifecycle_result < 0) {
            fprintf(stderr, "FLIP_CORE_HOST: after-frame lifecycle handling failed\n");
            host_result = -1;
            break;
        }
        if (lifecycle_after_frame.should_exit) {
            *exit_code = lifecycle_after_frame.exit_code;
            break;
        }

        uint64_t host_us = elapsed_us(frame_start_us);
        int wait_result = flip_timing_wait_frame(&timing);
        if (wait_result < 0) {
            fprintf(stderr, "FLIP_CORE_HOST: timing wait failed\n");
            host_result = -1;
            break;
        }

        double measured_fps = 0.0;
        int fps_result = flip_timing_record_frame(&timing, &measured_fps);
        if (fps_result < 0) {
            fprintf(stderr, "FLIP_CORE_HOST: timing FPS measurement failed\n");
            host_result = -1;
            break;
        }

        if (fps_result > 0) {
            last_measured_fps = measured_fps;
            last_measured_fps_valid = 1;
            if (telemetry_ready && perf_overlay_enabled &&
                flip_telemetry_sample(&telemetry, measured_fps, 1) == 0) {
                const flip_telemetry_snapshot_t *snapshot = flip_telemetry_snapshot(&telemetry);
                flip_video_set_perf_overlay(1, snapshot);
                log_telemetry(snapshot);
            }
            fprintf(stderr, "FLIP_CORE_HOST_FPS %.3f target=%.3f\n", measured_fps, fps);
        }

        uint64_t late_us = timing.last_late_ns > 0
            ? (uint64_t)timing.last_late_ns / 1000ULL : 0;

        flip_perf_diag_record_frame(
            &perf_diag,
            input_us,
            controls_us,
            core_input_us,
            core_run_us,
            0,
            audio_us,
            host_us,
            late_us,
            wait_result > 0);

        if (wait_result > 0)
            fprintf(stderr, "FLIP_CORE_HOST: frame schedule resync\n");
        if (fps_result > 0) {
            flip_audio_diag_snapshot_t audio_diag =
                log_audio_diag();

            uint64_t host_avg_us =
                flip_perf_metric_avg_us(
                    &perf_diag.host,
                    perf_diag.frames);

            if (dynamic_cpu.enabled) {
                unsigned previous_max_khz =
                    dynamic_cpu.current_max_khz;

                int dynamic_result =
                    flip_cpu_dynamic_sample(
                        &dynamic_cpu,
                        measured_fps,
                        fps,
                        host_avg_us,
                        audio_diag.queue_dropped_frames);

                if (dynamic_result == 1) {
                    fprintf(
                        stderr,
                        "FLIP_CPU_DYNAMIC: raised max_khz=%u->%u fps=%.3f target=%.3f host_avg_us=%llu audio_drops=%llu raise_count=%u learned_floor_khz=%u\n",
                        previous_max_khz,
                        dynamic_cpu.current_max_khz,
                        measured_fps,
                        fps,
                        (unsigned long long)host_avg_us,
                        (unsigned long long)audio_diag.queue_dropped_frames,
                        dynamic_cpu.raise_count,
                        dynamic_cpu.frequencies_khz[dynamic_cpu.base_index]);
                } else if (dynamic_result == 2) {
                    fprintf(
                        stderr,
                        "FLIP_CPU_DYNAMIC: lowered max_khz=%u->%u fps=%.3f target=%.3f good_windows=%u lower_count=%u\n",
                        previous_max_khz,
                        dynamic_cpu.current_max_khz,
                        measured_fps,
                        fps,
                        FLIP_CPU_DYNAMIC_GOOD_WINDOWS_TO_LOWER,
                        dynamic_cpu.lower_count);
                } else if (dynamic_result < 0) {
                    fprintf(
                        stderr,
                        "FLIP_CPU_DYNAMIC: policy update failed; disabling escalation\n");
                    dynamic_cpu.enabled = 0;
                }
            }

            log_perf_diag(&perf_diag, measured_fps);
            flip_perf_diag_reset(&perf_diag);
        }
    }

cleanup:
    if (cleanup_core(config->core, loaded, initialized) < 0 && host_result == 0)
        host_result = -1;

    return host_result;
}
