#!/usr/bin/env python3
from pathlib import Path

header = Path("include/flip_optimizer_calibration.h").read_text()
candidate = Path("src/hwtest/flip_optimizer_candidate_tool.c").read_text()
cal_tool = Path("src/hwtest/flip_optimizer_calibration_tool.c").read_text()

assert "#define FLIP_OPTIMIZER_CALIBRATION_RK3566_SAFE_MIN_UV 812500U" in header
assert "#define FLIP_OPTIMIZER_CALIBRATION_RK3566_MIN_TUNABLE_KHZ 408000U" in header
assert "#define FLIP_OPTIMIZER_VALIDATION_PARK_KHZ 408000U" in candidate
assert "flip_optimizer_calibration_preserve_stock_below(" in cal_tool
assert "FLIP_OPTIMIZER_CALIBRATION_RK3566_MIN_TUNABLE_KHZ" in cal_tool

print("optimizer full-range safe-floor architecture passed")
