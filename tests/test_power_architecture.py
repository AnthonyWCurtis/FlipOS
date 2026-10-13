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

input_source = (ROOT / "src/runtime/flip_input.c").read_text()
assert 'DEV_LID_OVERRIDE "/tmp/flipos-dev-lid-override"' in input_source
assert 'DEV_BUTTON_INJECT "/tmp/flipos-dev-button"' in input_source
assert 'apply_dev_button_injection(state)' in input_source
assert 'MENU VOL-' in input_source
assert 'case 116: return FLIP_BTN_POWER;' in input_source
assert '{"POWER", FLIP_BTN_POWER}' in input_source
assert '"POWER",' in input_source
assert 'strtok_r(value, " \\t\\r\\n"' in input_source
assert 'dev_button_release_mask' in input_source
assert 'strcmp(value, "open") == 0' in input_source
assert 'strcmp(value, "closed") == 0' in input_source
assert 'FLIP_DEV_LID_AUTO_OPEN' in source
assert 'dev_lid_auto_open_after_resume();' in source

print("power architecture tests passed")

updater = Path("tools/device/updater-playtest").read_text()
assert 'flip_rk817_charger.ko' in updater
assert 'flip-charge-service' in updater
assert 'insmod "$CHARGE_MODULE"' in updater
assert 'kill -0 "$OLD_CHARGE_PID"' in updater

assert 'FLIP_WIFI: supplicant started from saved config' in updater

assert 'FLIP_WIFI: dhcpcd rebind requested after association' in updater
