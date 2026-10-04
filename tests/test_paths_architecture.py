#!/usr/bin/env python3

from pathlib import Path

canonical = Path("include/flip_paths.h").read_text()

assert '"/mnt/SDCARD/flipos-dev"' in canonical
assert '"/mnt/SDCARD/Roms"' in canonical

service_files = [
    Path("include/flip_save.h"),
    Path("include/flip_state.h"),
    Path("include/flip_recovery.h"),
    Path("src/runtime/flip_settings.c"),
    Path("src/runtime/flip_library.c"),
    Path("src/runtime/flip_session.c"),
    Path("src/runtime/flip_session_registry.c"),
    Path("src/frontend/flip_frontend.c"),
]

for path in service_files:
    text = path.read_text()

    if "/mnt/SDCARD/flipos-dev" in text:
        raise AssertionError(
            f"raw FlipOS storage path remains in {path}"
        )

for path in [
    Path("include/flip_save.h"),
    Path("include/flip_state.h"),
    Path("include/flip_recovery.h"),
]:
    assert '#include "flip_paths.h"' in path.read_text()

assert '#include "flip_paths.h"' in Path(
    "src/runtime/flip_settings.c"
).read_text()

assert '#include "flip_paths.h"' in Path(
    "src/runtime/flip_library.c"
).read_text()

assert '#include "flip_paths.h"' in Path(
    "src/runtime/flip_session.c"
).read_text()

assert '#include "flip_paths.h"' in Path(
    "src/runtime/flip_session_registry.c"
).read_text()

assert "FLIP_RECENT_FILE" in Path(
    "src/frontend/flip_frontend.c"
).read_text()

makefile = Path("Makefile").read_text()
assert "$(BUILD)/flip_paths.o" in makefile
assert "test-paths" in makefile
assert "tests/test_paths_architecture.py" in makefile

print("paths/storage architecture test passed")
