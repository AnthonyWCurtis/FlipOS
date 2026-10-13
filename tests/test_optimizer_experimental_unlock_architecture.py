#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
frontend = (root / "src/frontend/flip_frontend.c").read_text()
makefile = (root / "Makefile").read_text()

assert "DEVELOPER_BUILD ?= 0" in makefile
assert "-DFLIPOS_DEVELOPER_BUILD=$(DEVELOPER_BUILD)" in makefile
assert "#define FLIPOS_DEVELOPER_BUILD 0" in frontend
assert "#define OPTIMIZER_EXPERIMENTAL_HOLD_SECONDS 4.0" in frontend
assert "SCREEN_OPTIMIZER_EXPERIMENTAL_WARNING" in frontend
for token in ("FLIP_BTN_L2", "FLIP_BTN_R2", "FLIP_BTN_START", "FLIP_BTN_SELECT"):
    assert token in frontend
assert "l2 && r2 && start && select" in frontend
assert "BELOW ROCKCHIP SPEC" in frontend
assert "DEVELOPMENT TEST ONLY" in frontend
assert "PRODUCTION PROFILE IS KEPT SEPARATE" in frontend
assert "FIRST TRIAL: 408MHZ @ 800MV ONLY" in frontend
assert "A ARM FIRST TRIAL   B BACK" in frontend
assert "FRONTEND_OPTIMIZER experimental unlock chord accepted" in frontend

print("optimizer experimental unlock architecture passed")
