#!/usr/bin/env python3

from pathlib import Path
import runpy

SOURCE = Path("src/runtime/flip_core_host.c").read_text()
TIMING_HEADER = Path("include/flip_timing.h").read_text()

assert "#include \"flip_perf_diag.h\"" in SOURCE
assert "FLIP_PERF:" in SOURCE
assert "flip_perf_diag_record_frame" in SOURCE
assert "last_late_ns" in TIMING_HEADER

assert "flip_core_save_memory_size(" in SOURCE, (
    "core host must inspect battery-backed save memory after game load"
)
assert "FLIP_SAVE: battery_memory size=%zu" in SOURCE, (
    "core host must log battery-backed save memory size for hardware validation"
)

run_start = SOURCE.index("int flip_core_host_run(")
while_pos = SOURCE.index("while (1)", run_start)
prefix = SOURCE[run_start:while_pos]

assert "flip_telemetry_sample(" not in prefix, (
    "hardware telemetry must not be sampled before the HUD is enabled"
)

fps_block = SOURCE.index("if (fps_result > 0)")
telemetry_sample = SOURCE.index("flip_telemetry_sample(", fps_block)
overlay_guard = SOURCE.rfind("perf_overlay_enabled", fps_block, telemetry_sample)

assert overlay_guard >= fps_block, (
    "periodic telemetry sampling must be guarded by perf_overlay_enabled"
)

runpy.run_path(
    "tests/test_video_present_diagnostics_architecture.py",
    run_name="__main__",
)

print("core host diagnostics boundary passed")
