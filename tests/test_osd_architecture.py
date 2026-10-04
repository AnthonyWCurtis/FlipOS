#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

header = (ROOT / "include/flip_osd.h").read_text()
source = (ROOT / "src/runtime/flip_osd.c").read_text()
frontend = (ROOT / "src/frontend/flip_frontend.c").read_text()
core_host = (ROOT / "src/runtime/flip_core_host.c").read_text()
makefile = (ROOT / "Makefile").read_text()

assert "void flip_osd_init(void);" in header
assert "void flip_osd_reset(void);" in header
assert "int flip_osd_snapshot(" in header

assert "flip_osd_init()" in frontend
assert "flip_osd_init()" in core_host

assert "static flip_osd_type_t current_type" in source
assert "static int poweroff_active" in source

assert "test-osd" in makefile
assert "test_osd_architecture.py" in makefile

print("osd architecture tests passed")
