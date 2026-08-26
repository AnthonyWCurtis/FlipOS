#!/usr/bin/env python3

from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def fail(message: str) -> None:
    print(f"phase4b perf diagnostics: {message}", file=sys.stderr)
    raise SystemExit(1)


def replace_once(path: str, old: str, new: str) -> None:
    target = ROOT / path
    text = target.read_text()
    count = text.count(old)
    if count != 1:
        fail(f"expected exactly one match in {path}, found {count}")
    target.write_text(text.replace(old, new, 1))
    print(f"updated {path}")


def branch_name() -> str:
    return subprocess.check_output(
        ["git", "branch", "--show-current"],
        cwd=ROOT,
        text=True,
    ).strip()


if branch_name() != "phase4/core-adapter-api":
    fail("run this only on phase4/core-adapter-api")

replace_once(
    "include/flip_timing.h",
    """    unsigned long long fps_frames;\n    double measured_fps;\n} flip_timing_t;\n""",
    """    unsigned long long fps_frames;\n    double measured_fps;\n\n    /* Last wake lateness relative to the absolute frame deadline. */\n    long long last_late_ns;\n} flip_timing_t;\n""",
)

replace_once(
    "src/runtime/flip_timing.c",
    """static double timespec_elapsed_seconds(\n""",
    """static long long timespec_diff_ns(\n    const struct timespec *start,\n    const struct timespec *end)\n{\n    return\n        (long long)(end->tv_sec - start->tv_sec) *\n            1000000000LL +\n        (long long)(end->tv_nsec - start->tv_nsec);\n}\n\nstatic double timespec_elapsed_seconds(\n""",
)

replace_once(
    "src/runtime/flip_timing.c",
    """    timing->fps_frames = 0;\n    timing->measured_fps = 0.0;\n\n    return 0;\n""",
    """    timing->fps_frames = 0;\n    timing->measured_fps = 0.0;\n    timing->last_late_ns = 0;\n\n    return 0;\n""",
)

replace_once(
    "src/runtime/flip_timing.c",
    """    if (clock_gettime(\n            CLOCK_MONOTONIC,\n            &now) < 0)\n        return -1;\n\n    late_limit =\n        timing->next_frame;\n""",
    """    if (clock_gettime(\n            CLOCK_MONOTONIC,\n            &now) < 0)\n        return -1;\n\n    timing->last_late_ns =\n        timespec_after(\n            &now,\n            &timing->next_frame)\n        ? timespec_diff_ns(\n            &timing->next_frame,\n            &now)\n        : 0;\n\n    late_limit =\n        timing->next_frame;\n""",
)

replace_once(
    "tests/test_flip_timing.c",
    """    assert(\n        flip_timing_wait_frame(\n            &timing) == 1);\n\n    timing.fps_frames = 42;\n""",
    """    assert(\n        flip_timing_wait_frame(\n            &timing) == 1);\n\n    assert(\n        timing.last_late_ns >\n        4000000000LL);\n\n    timing.fps_frames = 42;\n""",
)

replace_once(
    "tests/test_flip_timing.c",
    """    assert(\n        timing.measured_fps == 0.0);\n\n    puts(\n""",
    """    assert(\n        timing.measured_fps == 0.0);\n\n    assert(\n        timing.last_late_ns == 0);\n\n    puts(\n""",
)

replace_once(
    "src/runtime/flip_core_host.c",
    """#include \"flip_core_host.h\"\n""",
    """#define _POSIX_C_SOURCE 200809L\n\n#include \"flip_core_host.h\"\n""",
)

replace_once(
    "src/runtime/flip_core_host.c",
    """#include \"flip_lifecycle.h\"\n#include \"flip_telemetry.h\"\n""",
    """#include \"flip_lifecycle.h\"\n#include \"flip_perf_diag.h\"\n#include \"flip_telemetry.h\"\n""",
)

replace_once(
    "src/runtime/flip_core_host.c",
    """#include <stdio.h>\n#include <string.h>\n""",
    """#include <stdint.h>\n#include <stdio.h>\n#include <string.h>\n#include <time.h>\n""",
)

replace_once(
    "src/runtime/flip_core_host.c",
    """int flip_core_host_run(\n""",
    """static uint64_t monotonic_us(void)\n{\n    struct timespec now;\n\n    if (clock_gettime(\n            CLOCK_MONOTONIC,\n            &now) < 0) {\n\n        return 0;\n    }\n\n    return\n        (uint64_t)now.tv_sec *\n            1000000ULL +\n        (uint64_t)now.tv_nsec /\n            1000ULL;\n}\n\nstatic uint64_t elapsed_us(\n    uint64_t start_us)\n{\n    uint64_t now_us = monotonic_us();\n\n    if (now_us < start_us)\n        return 0;\n\n    return now_us - start_us;\n}\n\nstatic void log_perf_diag(\n    const flip_perf_diag_t *diag,\n    double measured_fps)\n{\n    if (!diag ||\n        diag->frames == 0)\n        return;\n\n    fprintf(\n        stderr,\n        \"FLIP_PERF: fps=%.3f frames=%llu \"\n        \"input_avg_us=%llu input_max_us=%llu \"\n        \"controls_avg_us=%llu controls_max_us=%llu \"\n        \"core_input_avg_us=%llu core_input_max_us=%llu \"\n        \"core_run_avg_us=%llu core_run_max_us=%llu \"\n        \"audio_avg_us=%llu audio_max_us=%llu \"\n        \"host_avg_us=%llu host_max_us=%llu \"\n        \"late_avg_us=%llu late_max_us=%llu \"\n        \"late_frames=%llu resyncs=%llu\\n\",\n        measured_fps,\n        (unsigned long long)diag->frames,\n        (unsigned long long)flip_perf_metric_avg_us(\n            &diag->input, diag->frames),\n        (unsigned long long)diag->input.max_us,\n        (unsigned long long)flip_perf_metric_avg_us(\n            &diag->controls, diag->frames),\n        (unsigned long long)diag->controls.max_us,\n        (unsigned long long)flip_perf_metric_avg_us(\n            &diag->core_input, diag->frames),\n        (unsigned long long)diag->core_input.max_us,\n        (unsigned long long)flip_perf_metric_avg_us(\n            &diag->core_run, diag->frames),\n        (unsigned long long)diag->core_run.max_us,\n        (unsigned long long)flip_perf_metric_avg_us(\n            &diag->audio, diag->frames),\n        (unsigned long long)diag->audio.max_us,\n        (unsigned long long)flip_perf_metric_avg_us(\n            &diag->host, diag->frames),\n        (unsigned long long)diag->host.max_us,\n        (unsigned long long)flip_perf_metric_avg_us(\n            &diag->late, diag->late_frames),\n        (unsigned long long)diag->late.max_us,\n        (unsigned long long)diag->late_frames,\n        (unsigned long long)diag->resyncs);\n}\n\nint flip_core_host_run(\n""",
)

replace_once(
    "src/runtime/flip_core_host.c",
    """    if (telemetry_ready) {\n        flip_telemetry_sample(\n            &telemetry,\n            0.0,\n            0);\n    } else {\n        fprintf(\n            stderr,\n            \"FLIP_TELEMETRY: initialization failed\\n\");\n    }\n""",
    """    if (!telemetry_ready) {\n        fprintf(\n            stderr,\n            \"FLIP_TELEMETRY: initialization failed\\n\");\n    }\n\n    flip_perf_diag_t perf_diag;\n    flip_perf_diag_reset(&perf_diag);\n""",
)

replace_once(
    "src/runtime/flip_core_host.c",
    """    int perf_overlay_enabled = 0;\n\n    persistence_bridge_t bridge = {\n""",
    """    int perf_overlay_enabled = 0;\n    double last_measured_fps = 0.0;\n    int last_fps_valid = 0;\n\n    persistence_bridge_t bridge = {\n""",
)

replace_once(
    "src/runtime/flip_core_host.c",
    """    while (1) {\n        if (flip_input_poll(\n                &physical_input,\n                0) < 0) {\n\n            fprintf(\n                stderr,\n                \"FLIP_CORE_HOST: input poll failed\\n\");\n            host_result = -1;\n            break;\n        }\n""",
    """    while (1) {\n        uint64_t frame_start_us = monotonic_us();\n        uint64_t stage_start_us = frame_start_us;\n        uint64_t input_us = 0;\n        uint64_t controls_us = 0;\n        uint64_t core_input_us = 0;\n        uint64_t core_run_us = 0;\n        uint64_t audio_us = 0;\n\n        int input_result =\n            flip_input_poll(\n                &physical_input,\n                0);\n\n        input_us = elapsed_us(stage_start_us);\n\n        if (input_result < 0) {\n            fprintf(\n                stderr,\n                \"FLIP_CORE_HOST: input poll failed\\n\");\n            host_result = -1;\n            break;\n        }\n""",
)

replace_once(
    "src/runtime/flip_core_host.c",
    """        if (flip_controls_update(\n                &controls,\n                &physical_input,\n                &control_state) < 0) {\n\n            fprintf(\n                stderr,\n                \"FLIP_CORE_HOST: controls update failed\\n\");\n            host_result = -1;\n            break;\n        }\n""",
    """        stage_start_us = monotonic_us();\n\n        int controls_result =\n            flip_controls_update(\n                &controls,\n                &physical_input,\n                &control_state);\n\n        controls_us = elapsed_us(stage_start_us);\n\n        if (controls_result < 0) {\n            fprintf(\n                stderr,\n                \"FLIP_CORE_HOST: controls update failed\\n\");\n            host_result = -1;\n            break;\n        }\n""",
)

replace_once(
    "src/runtime/flip_core_host.c",
    """        if (flip_core_set_input(\n                config->core,\n                &control_state.game) < 0) {\n\n            fprintf(\n                stderr,\n                \"FLIP_CORE_HOST: core input update failed\\n\");\n            host_result = -1;\n            break;\n        }\n""",
    """        stage_start_us = monotonic_us();\n\n        int core_input_result =\n            flip_core_set_input(\n                config->core,\n                &control_state.game);\n\n        core_input_us = elapsed_us(stage_start_us);\n\n        if (core_input_result < 0) {\n            fprintf(\n                stderr,\n                \"FLIP_CORE_HOST: core input update failed\\n\");\n            host_result = -1;\n            break;\n        }\n""",
)

replace_once(
    "src/runtime/flip_core_host.c",
    """        if (flip_core_run_frame(\n                config->core) < 0) {\n\n            fprintf(\n                stderr,\n                \"FLIP_CORE_HOST: core frame failed\\n\");\n            host_result = -1;\n            break;\n        }\n\n        if (flip_audio_end_frame() < 0) {\n            fprintf(\n                stderr,\n                \"FLIP_CORE_HOST: audio frame flush failed\\n\");\n        }\n""",
    """        stage_start_us = monotonic_us();\n\n        int core_run_result =\n            flip_core_run_frame(\n                config->core);\n\n        core_run_us = elapsed_us(stage_start_us);\n\n        if (core_run_result < 0) {\n            fprintf(\n                stderr,\n                \"FLIP_CORE_HOST: core frame failed\\n\");\n            host_result = -1;\n            break;\n        }\n\n        stage_start_us = monotonic_us();\n\n        int audio_result =\n            flip_audio_end_frame();\n\n        audio_us = elapsed_us(stage_start_us);\n\n        if (audio_result < 0) {\n            fprintf(\n                stderr,\n                \"FLIP_CORE_HOST: audio frame flush failed\\n\");\n        }\n""",
)

replace_once(
    "src/runtime/flip_core_host.c",
    """        if (control_state.system.fps_toggle_requested) {\n            perf_overlay_enabled =\n                !perf_overlay_enabled;\n\n            flip_video_set_perf_overlay(\n                perf_overlay_enabled,\n                telemetry_ready\n                    ? flip_telemetry_snapshot(\n                        &telemetry)\n                    : NULL);\n\n            fprintf(\n                stderr,\n                \"FLIP_CORE_HOST: performance overlay %s\\n\",\n                perf_overlay_enabled\n                    ? \"ON\"\n                    : \"OFF\");\n        }\n""",
    """        if (control_state.system.fps_toggle_requested) {\n            perf_overlay_enabled =\n                !perf_overlay_enabled;\n\n            if (perf_overlay_enabled &&\n                telemetry_ready) {\n\n                if (flip_telemetry_sample(\n                        &telemetry,\n                        last_measured_fps,\n                        last_fps_valid) == 0) {\n\n                    const flip_telemetry_snapshot_t *snapshot =\n                        flip_telemetry_snapshot(\n                            &telemetry);\n\n                    flip_video_set_perf_overlay(\n                        1,\n                        snapshot);\n\n                    log_telemetry(snapshot);\n                }\n            } else {\n                flip_video_set_perf_overlay(\n                    0,\n                    NULL);\n            }\n\n            fprintf(\n                stderr,\n                \"FLIP_CORE_HOST: performance overlay %s\\n\",\n                perf_overlay_enabled\n                    ? \"ON\"\n                    : \"OFF\");\n        }\n""",
)

replace_once(
    "src/runtime/flip_core_host.c",
    """        if (fps_result > 0) {\n            if (telemetry_ready) {\n                if (flip_telemetry_sample(\n                        &telemetry,\n                        measured_fps,\n                        1) == 0) {\n\n                    const flip_telemetry_snapshot_t *snapshot =\n                        flip_telemetry_snapshot(\n                            &telemetry);\n\n                    flip_video_set_perf_overlay(\n                        perf_overlay_enabled,\n                        snapshot);\n\n                    log_telemetry(snapshot);\n                }\n            }\n\n            fprintf(\n                stderr,\n                \"FLIP_CORE_HOST_FPS %.3f target=%.3f\\n\",\n                measured_fps,\n                fps);\n        }\n""",
    """        int perf_report_due =\n            fps_result > 0;\n\n        if (fps_result > 0) {\n            last_measured_fps = measured_fps;\n            last_fps_valid = 1;\n\n            if (perf_overlay_enabled &&\n                telemetry_ready) {\n\n                if (flip_telemetry_sample(\n                        &telemetry,\n                        measured_fps,\n                        1) == 0) {\n\n                    const flip_telemetry_snapshot_t *snapshot =\n                        flip_telemetry_snapshot(\n                            &telemetry);\n\n                    flip_video_set_perf_overlay(\n                        1,\n                        snapshot);\n\n                    log_telemetry(snapshot);\n                }\n            }\n\n            fprintf(\n                stderr,\n                \"FLIP_CORE_HOST_FPS %.3f target=%.3f\\n\",\n                measured_fps,\n                fps);\n        }\n""",
)

replace_once(
    "src/runtime/flip_core_host.c",
    """        int wait_result =\n            flip_timing_wait_frame(\n                &timing);\n\n        if (wait_result < 0) {\n            fprintf(\n                stderr,\n                \"FLIP_CORE_HOST: timing wait failed\\n\");\n            host_result = -1;\n            break;\n        }\n\n        if (wait_result > 0) {\n            fprintf(\n                stderr,\n                \"FLIP_CORE_HOST: frame schedule resync\\n\");\n        }\n""",
    """        uint64_t host_us =\n            elapsed_us(frame_start_us);\n\n        int wait_result =\n            flip_timing_wait_frame(\n                &timing);\n\n        if (wait_result < 0) {\n            fprintf(\n                stderr,\n                \"FLIP_CORE_HOST: timing wait failed\\n\");\n            host_result = -1;\n            break;\n        }\n\n        uint64_t late_us =\n            timing.last_late_ns > 0\n                ? (uint64_t)timing.last_late_ns /\n                    1000ULL\n                : 0;\n\n        /*\n         * core_run_us includes the core and callbacks it invokes,\n         * including video submission. If this bucket is the culprit,\n         * the next probe will split video presentation from emulation.\n         */\n        flip_perf_diag_record_frame(\n            &perf_diag,\n            input_us,\n            controls_us,\n            core_input_us,\n            core_run_us,\n            0,\n            audio_us,\n            host_us,\n            late_us,\n            wait_result > 0);\n\n        if (wait_result > 0) {\n            fprintf(\n                stderr,\n                \"FLIP_CORE_HOST: frame schedule resync\\n\");\n        }\n\n        if (perf_report_due) {\n            log_perf_diag(\n                &perf_diag,\n                measured_fps);\n\n            flip_perf_diag_reset(\n                &perf_diag);\n        }\n""",
)

replace_once(
    "Makefile",
    """\t$(BUILD)/flip_core_host.o \\\n\t$(BUILD)/flip_telemetry.o \\\n""",
    """\t$(BUILD)/flip_core_host.o \\\n\t$(BUILD)/flip_perf_diag.o \\\n\t$(BUILD)/flip_telemetry.o \\\n""",
)

replace_once(
    "Makefile",
    """.PHONY: all frontend gb test-controls test-timing test-audio test-lifecycle test-core test-core-host test-telemetry test-architecture clean clean-objects\n""",
    """.PHONY: all frontend gb test-controls test-timing test-audio test-lifecycle test-core test-core-host test-perf-diag test-telemetry test-architecture clean clean-objects\n""",
)

replace_once(
    "Makefile",
    """test-core-host: $(BUILD)/test-flip-core-host\n\t$(BUILD)/test-flip-core-host\n\ntest-telemetry: $(BUILD)/test-flip-telemetry\n""",
    """test-core-host: $(BUILD)/test-flip-core-host\n\t$(BUILD)/test-flip-core-host\n\ntest-perf-diag: $(BUILD)/test-flip-perf-diag\n\t$(BUILD)/test-flip-perf-diag\n\ntest-telemetry: $(BUILD)/test-flip-telemetry\n""",
)

replace_once(
    "Makefile",
    """test-architecture:\n\tpython3 tests/test_flip_gb_architecture.py\n\tpython3 tests/test_session_adapter_architecture.py\n""",
    """test-architecture:\n\tpython3 tests/test_flip_gb_architecture.py\n\tpython3 tests/test_session_adapter_architecture.py\n\tpython3 tests/test_core_host_diagnostics_architecture.py\n""",
)

replace_once(
    "Makefile",
    """$(BUILD)/test-flip-core-host: tests/test_flip_core_host.c tests/test_flip_core_host_telemetry_stubs.c src/runtime/flip_core.c src/runtime/flip_core_host.c include/flip_core_host.h | $(BUILD)\n\t$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \\\n\t\ttests/test_flip_core_host.c tests/test_flip_core_host_telemetry_stubs.c \\\n\t\tsrc/runtime/flip_core.c src/runtime/flip_core_host.c \\\n\t\t-o $@\n\n$(BUILD)/test-flip-telemetry: tests/test_flip_telemetry.c src/runtime/flip_telemetry.c include/flip_telemetry.h | $(BUILD)\n""",
    """$(BUILD)/test-flip-core-host: tests/test_flip_core_host.c tests/test_flip_core_host_telemetry_stubs.c src/runtime/flip_core.c src/runtime/flip_core_host.c src/runtime/flip_perf_diag.c include/flip_core_host.h include/flip_perf_diag.h | $(BUILD)\n\t$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \\\n\t\ttests/test_flip_core_host.c tests/test_flip_core_host_telemetry_stubs.c \\\n\t\tsrc/runtime/flip_core.c src/runtime/flip_core_host.c src/runtime/flip_perf_diag.c \\\n\t\t-o $@\n\n$(BUILD)/test-flip-perf-diag: tests/test_flip_perf_diag.c src/runtime/flip_perf_diag.c include/flip_perf_diag.h | $(BUILD)\n\t$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \\\n\t\ttests/test_flip_perf_diag.c src/runtime/flip_perf_diag.c \\\n\t\t-o $@\n\n$(BUILD)/test-flip-telemetry: tests/test_flip_telemetry.c src/runtime/flip_telemetry.c include/flip_telemetry.h | $(BUILD)\n""",
)

replace_once(
    "Makefile",
    """$(BUILD)/flip_core_host.o: src/runtime/flip_core_host.c | $(BUILD)\n\t$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@\n\n$(BUILD)/flip_telemetry.o: src/runtime/flip_telemetry.c | $(BUILD)\n""",
    """$(BUILD)/flip_core_host.o: src/runtime/flip_core_host.c | $(BUILD)\n\t$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@\n\n$(BUILD)/flip_perf_diag.o: src/runtime/flip_perf_diag.c | $(BUILD)\n\t$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@\n\n$(BUILD)/flip_telemetry.o: src/runtime/flip_telemetry.c | $(BUILD)\n""",
)

replace_once(
    "Makefile",
    """\t\t$(BUILD)/test-flip-core-host \\\n\t\t$(BUILD)/test-flip-telemetry\n""",
    """\t\t$(BUILD)/test-flip-core-host \\\n\t\t$(BUILD)/test-flip-perf-diag \\\n\t\t$(BUILD)/test-flip-telemetry\n""",
)

print("phase4b performance diagnostics patch applied")
print("run: make test-timing test-core-host test-perf-diag test-architecture && make all")
