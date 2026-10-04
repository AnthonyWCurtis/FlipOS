#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

header = (ROOT / "include/flip_save.h").read_text()
source = (ROOT / "src/runtime/flip_save.c").read_text()
gb = (ROOT / "src/emulators/flip_gb.c").read_text()
makefile = (ROOT / "Makefile").read_text()

assert "flip_save_status_t" in header
assert "flip_save_query_status(" in header
assert "flip_save_query_status(" in source

assert "flip_save_query_status(" in gb
assert "flip_save_health(" not in gb

assert "test-save-manager" in makefile
assert "test_save_manager_architecture.py" in makefile

print("save manager architecture tests passed")
