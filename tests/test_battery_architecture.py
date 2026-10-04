from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

battery = (ROOT / "src/runtime/flip_battery.c").read_text()
telemetry = (ROOT / "src/runtime/flip_telemetry.c").read_text()
telemetry_h = (ROOT / "include/flip_telemetry.h").read_text()
makefile = (ROOT / "Makefile").read_text()

assert '#include "flip_battery.h"' in telemetry
assert "flip_battery_read(" in telemetry
assert "discover_battery_path" not in telemetry
assert "battery_path" not in telemetry
assert "battery_path" not in telemetry_h

assert "/sys/class/power_supply/battery" not in battery
assert "FLIP_BATTERY_ROOT" in battery

assert "test-battery" in makefile
assert "tests/test_battery_architecture.py" in makefile
assert "flip_battery.o" in makefile

print("battery architecture: PASS")
