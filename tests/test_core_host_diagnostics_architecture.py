#!/usr/bin/env python3

from pathlib import Path

SOURCE = Path("src/runtime/flip_core_host.c").read_text()
TIMING_HEADER = Path("include/flip_timing.h").read_text()

assert "#include \"flip_perf_diag.h\"" in SOURCE
assert "FLIP_PERF:" in SOURCE
assert "flip_perf_diag_record_frame" in SOURCE
assert "last_late_ns" in TIMING_HEADER

while_pos = SOURCE.index("while (1)")
prefix = SOURCE[:while_pos]

assert "flip_telemetry_sample(" not in prefix, (
    "hardware telemetry must not be sampled before the HUD is enabled"
)

fps_block = SOURCE.index("if (fps_result > 0)")
telemetry_sample = SOURCE.index("flip_telemetry_sample(", fps_block)
overlay_guard = SOURCE.rfind("perf_overlay_enabled", fps_block, telemetry_sample)

assert overlay_guard >= fps_block, (
    "periodic telemetry sampling must be guarded by perf_overlay_enabled"
)

print("core host diagnostics boundary passed")
