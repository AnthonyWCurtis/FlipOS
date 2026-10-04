#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

header = (ROOT / "include/flip_settings.h").read_text()
frontend = (ROOT / "src/frontend/flip_frontend.c").read_text()
audio = (ROOT / "src/audio/flip_audio.c").read_text()
core_host = (ROOT / "src/runtime/flip_core_host.c").read_text()
makefile = (ROOT / "Makefile").read_text()

assert "int flip_settings_init(void);" in header
assert "int flip_settings_is_ready(void);" in header

assert "flip_settings_init()" in frontend
assert "flip_settings_init()" in core_host

assert "flip_settings_load(" not in frontend
assert "flip_settings_load(" not in audio

assert "flip_settings_get_volume()" in audio
assert "flip_settings_get_brightness()" in frontend

assert "test-settings" in makefile
assert "test_settings_architecture.py" in makefile

print("settings architecture tests passed")
