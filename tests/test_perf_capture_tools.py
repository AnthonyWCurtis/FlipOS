#!/usr/bin/env python3

import importlib.util
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src/hwtest/flip_perf_capture.c").read_text()
MAKEFILE = (ROOT / "Makefile").read_text()

assert '#include "flip_telemetry.h"' in SOURCE
assert '#include "flip_battery.h"' in SOURCE
assert "flip_telemetry_sample(" in SOURCE
assert "flip_battery_read(" in SOURCE

# The capture utility must remain low-overhead: do not regress to spawning
# shell utilities for each sample.
for forbidden in ("system(", "popen(", "fork(", "execl(", "execv("):
    assert forbidden not in SOURCE, f"capture tool must not use {forbidden}"

assert "perf-capture: $(BUILD)/flip-perf-capture" in MAKEFILE
assert "$(BUILD)/flip-perf-capture: $(PERF_CAPTURE_OBJS)" in MAKEFILE

summary_path = ROOT / "tools/flip-perf-summary.py"
spec = importlib.util.spec_from_file_location("flip_perf_summary", summary_path)
assert spec and spec.loader
summary = importlib.util.module_from_spec(spec)
spec.loader.exec_module(summary)

header = (
    "epoch,uptime_s,label,live_mode,live_runner_pid,"
    "session_state,session_system,session_pid,"
    "volume_percent,brightness_saved,brightness_actual,"
    "cpu_percent,cpu_khz,cpu_governor,"
    "gpu_percent,gpu_hz,gpu_governor,"
    "soc_temp_mc,gpu_temp_mc,"
    "battery_percent,battery_status,battery_uv,battery_ua,"
    "battery_power_mw,charge_counter,"
    "frontend_pid,gpu_service_pid,aplay_pid\n"
)

rows = [
    "100,10,test,FRONTEND,,closed,GBC,1,0,200,200,1,408000,ondemand,0,200000000,"
    "simple_ondemand,40000,39000,80,DISCHARGING,4000000,-100000,-400,1,2,,\n",
    "101,11,test,FRONTEND,,closed,GBC,1,0,200,200,2,408000,ondemand,0,200000000,"
    "simple_ondemand,41000,40000,80,DISCHARGING,3990000,-100000,-400,1,2,,\n",
    "102,12,test,FRONTEND,,closed,GBC,1,0,200,200,3,408000,ondemand,0,200000000,"
    "simple_ondemand,42000,41000,80,DISCHARGING,3980000,-100000,-400,1,2,,\n",
]

with tempfile.NamedTemporaryFile(
    mode="w",
    suffix=".csv",
    delete=False,
) as handle:
    handle.write(header)
    handle.writelines(rows)
    fixture = Path(handle.name)

try:
    import csv

    with fixture.open(newline="", encoding="utf-8") as handle:
        parsed = list(csv.DictReader(handle))

    mah, mwh = summary.integrate_discharge(parsed)

    assert abs(mah - (200000 / 3_600_000.0)) < 1e-9
    assert abs(mwh - (800 / 3600.0)) < 1e-9
finally:
    fixture.unlink(missing_ok=True)

print("performance capture tools: PASS")
