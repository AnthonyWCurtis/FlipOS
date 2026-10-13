#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
hdr = (root / "include/flip_optimizer_calibration.h").read_text()
cal = (root / "src/hwtest/flip_optimizer_calibration_tool.c").read_text()
cand = (root / "src/hwtest/flip_optimizer_candidate_tool.c").read_text()
front = (root / "src/frontend/flip_frontend.c").read_text()
mk = (root / "Makefile").read_text()
updater = (root / "tools/device/updater-playtest").read_text()

assert "FLIP_OPTIMIZER_CALIBRATION_RK3566_SAFE_MIN_UV 812500U" in hdr
assert "FLIP_OPTIMIZER_CALIBRATION_RK3566_FLOOR_UV" in hdr
assert "flip_optimizer_calibration_load_file(" in hdr
assert "FLIP_OPTIMIZER_CALIBRATION_TRIAL_PREFIX" in hdr

assert "init-experimental-first" in cal
assert "first_frequency_khz=408000" in cal
assert "first_candidate_uv=800000" in cal
assert "FLIP_OPTIMIZER_CALIBRATION_RK3566_SAFE_MIN_UV" in cal

for token in [
    "cpu-calibration-experimental",
    "cpu-result-experimental",
    "cpu-report-experimental",
    "FLIP_OPTIMIZER_CALIBRATION_RK3566_FLOOR_UV=750000U",
    "flip-optimizer-calibration-experimental",
    "flip-optimizer-candidate-experimental",
]:
    assert token in mk, token

for token in [
    "experimental-trial-request",
    "flip-optimizer-calibration-experimental",
    "flip-optimizer-candidate-experimental",
    "live-once 20",
]:
    assert token in updater, token

for token in [
    "RUN 408MHZ @ 800MV?",
    "FIRST TRIAL: 408MHZ @ 800MV ONLY",
    "WATCHDOG + PANIC + TSADC RECOVERY ARMED",
    "experimental first trial requested",
]:
    assert token in front, token

assert "#ifndef FLIP_OPTIMIZER_UI_RETURN_FILE" in cand
assert "#ifndef FLIP_OPTIMIZER_AUTO_SESSION_FILE" in cand

print("optimizer experimental backend architecture passed")
