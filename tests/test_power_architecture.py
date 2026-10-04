#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

header = (ROOT / "include/flip_power.h").read_text()
source = (ROOT / "src/runtime/flip_power.c").read_text()
frontend = (ROOT / "src/frontend/flip_frontend.c").read_text()
makefile = (ROOT / "Makefile").read_text()

assert "typedef enum" in header
assert "FLIP_POWER_LID_ERROR" in header
assert "FLIP_POWER_LID_UNKNOWN" in header
assert "FLIP_POWER_LID_OPEN" in header
assert "FLIP_POWER_LID_CLOSED" in header
assert "flip_power_refresh_lid_state(" in header

assert 'FLIP_BACKLIGHT_BRIGHTNESS' in source
assert 'flip_power_refresh_lid_state(' in source

assert "flip_power_refresh_lid_state(" in frontend
assert "static int wait_for_lid_open(" not in frontend

assert "test-power" in makefile
assert "test_power_architecture.py" in makefile

print("power architecture tests passed")
