from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

header = (ROOT / "include/flip_library.h").read_text(encoding="utf-8")
source = (ROOT / "src/runtime/flip_library.c").read_text(encoding="utf-8")
frontend = (ROOT / "src/frontend/flip_frontend.c").read_text(encoding="utf-8")
makefile = (ROOT / "Makefile").read_text(encoding="utf-8")

assert "flip_library_cache_state_t" in header
assert "flip_library_cache_status_t" in header
assert "flip_library_cache_evaluate(" in header
assert "flip_library_commit_generation(" in header

assert "flip_library_cache_evaluate(" in source
assert "flip_library_commit_generation(" in source

assert "flip_library_cache_evaluate(" in frontend
assert "flip_library_commit_generation(" in frontend
assert "update_system_fingerprint(" not in frontend

for forbidden in [
    "flip_library_fingerprint(",
    "flip_library_load_fingerprint(",
    "flip_library_save_fingerprint(",
    "flip_library_save_index(",
]:
    assert forbidden not in frontend

assert "flip_library_load_index(" in frontend
assert "test-library-cache" in makefile

print("library cache ownership test passed")
