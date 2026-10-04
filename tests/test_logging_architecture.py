from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

log_source = (ROOT / "src/runtime/flip_log.c").read_text()
lifecycle = (ROOT / "src/runtime/flip_lifecycle.c").read_text()
frontend = (ROOT / "src/frontend/flip_frontend.c").read_text()
paths = (ROOT / "include/flip_paths.h").read_text()
makefile = (ROOT / "Makefile").read_text()

assert '#include "flip_paths.h"' in log_source
assert 'flip_paths_log_root()' in log_source
assert '/mnt/SDCARD/flipos-dev/logs' not in log_source

assert '#include "flip_log.h"' in lifecycle
assert 'flip_log_write(' in lifecycle
assert 'fprintf(' not in lifecycle
assert 'stderr' not in lifecycle

assert '#include "flip_log.h"' in frontend
assert 'flip_log_write(' in frontend

assert 'FLIP_LOG_ROOT' in paths
assert 'flip_paths_log_root' in paths

assert 'test-log' in makefile
assert 'tests/test_logging_architecture.py' in makefile
assert 'flip_log.o' in makefile

print("logging architecture: PASS")
