from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

source = (ROOT / "src/runtime/flip_performance.c").read_text()
header = (ROOT / "include/flip_performance.h").read_text()
makefile = (ROOT / "Makefile").read_text()

assert "FLIP_PERFORMANCE_BALANCED" in header
assert "FLIP_PERFORMANCE_PERFORMANCE" in header
assert "FLIP_PERFORMANCE_POWERSAVE" in header

assert "scaling_governor" in source
assert "scaling_available_governors" in source
assert "available_governors" in source
assert "flip_performance_profile_supported(" in source
assert "flip_performance_restore(" in source

assert "/sys/devices/system/cpu" not in source
assert "/sys/class/devfreq" not in source

assert "scaling_max_freq" not in source
assert "scaling_min_freq" not in source
assert "max_freq" not in source
assert "min_freq" not in source

assert "test-performance" in makefile
assert "tests/test_performance_architecture.py" in makefile
assert "flip_performance.o" in makefile

print("performance architecture: PASS")
