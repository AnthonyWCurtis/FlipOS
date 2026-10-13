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

progress = Path("src/hwtest/flip_optimizer_progress.c").read_text()
validation_progress = Path("src/hwtest/flip_optimizer_validation_progress.c").read_text()
runtime_tool = Path("src/hwtest/flip_optimizer_runtime_tool.c").read_text()

assert "#define MIN_TUNABLE_KHZ 408000U" in progress
assert "#define VALIDATION_LEVELS 8U" in validation_progress
for khz in (1992000, 1800000, 1608000, 1416000, 1104000, 816000, 600000, 408000):
    assert f"{khz}U" in validation_progress
assert "const int box_w = 64;" in validation_progress
assert "const int gap = 6;" in validation_progress
assert "restore_all_with_handoff(" in runtime_tool
assert "opp->frequency_khz == PARK_KHZ" in runtime_tool

print("optimizer full-range safe-floor architecture passed")
