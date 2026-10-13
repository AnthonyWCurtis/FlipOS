#!/usr/bin/env python3
"""Architecture and pixel-layout guardrails for the FlipOS Power Menu."""

from pathlib import Path
import re

root = Path(".")
frontend = (root / "src/frontend/flip_frontend.c").read_text()
controls_h = (root / "include/flip_controls.h").read_text()
controls = (root / "src/runtime/flip_controls.c").read_text()
input_h = (root / "include/flip_input.h").read_text()
input_c = (root / "src/runtime/flip_input.c").read_text()
power_h = (root / "include/flip_power.h").read_text()
power_c = (root / "src/runtime/flip_power.c").read_text()
video_h = (root / "include/flip_video.h").read_text()
video = (root / "src/runtime/flip_video.c").read_text()
host = (root / "src/runtime/flip_core_host.c").read_text()


def define_int(source: str, name: str) -> int:
    match = re.search(rf"^#define\s+{re.escape(name)}\s+(\d+)\s*$", source, re.M)
    assert match, name
    return int(match.group(1))


# Physical POWER is independent from the hall sensor: the RK817 PMIC key is
# exposed as event2, while event1 remains the lid-only wake path.
assert "FLIP_BTN_POWER" in input_h
assert '#define POWER_DEVICE   "/dev/input/event2"' in input_c
assert 'open(POWER_DEVICE, O_RDONLY | O_NONBLOCK)' in input_c
assert 'ev.code != KEY_POWER' in input_c
assert 'state->down[FLIP_BTN_POWER]' in input_c
assert 'HALL_DEVICE    "/dev/input/event1"' in input_c

# Short press is recognized on release; a hold past the tap window is cancelled
# so userspace does not replace the physical emergency long-hold behaviour.
assert "power_menu_requested" in controls_h
assert "#define FLIP_POWER_MENU_TAP_MAX_MS 700ULL" in controls
assert "controls->power_pending = 0;" in controls
assert "state->system.power_menu_requested = 1;" in controls
assert "held_ms <=" in controls

# One-shot sleep is distinct from lid-close sleep.
assert "flip_power_sleep_once(void);" in power_h
assert "flip_power_sleep_once(void)" in power_c
assert 'FLIP_POWER_MEM_SLEEP,\n            "deep"' in power_c
assert 'FLIP_POWER_STATE,\n            "mem"' in power_c

# Frontend Power Menu geometry.
margin = define_int(frontend, "POWER_MENU_MARGIN_X")
content = define_int(frontend, "POWER_MENU_CONTENT_W")
panel_y = define_int(frontend, "POWER_MENU_PANEL_Y")
row_y = define_int(frontend, "POWER_MENU_ROW_Y")
row_h = define_int(frontend, "POWER_MENU_ROW_H")
gap = define_int(frontend, "POWER_MENU_ROW_GAP")
confirm_y = define_int(frontend, "POWER_MENU_CONFIRM_Y")
footer_y = define_int(frontend, "POWER_MENU_FOOTER_Y")

assert margin + content <= 640 - margin
assert panel_y < row_y
assert row_y + 3 * row_h + 2 * gap < confirm_y
assert confirm_y + 72 < footer_y
assert footer_y + 7 <= 480

for token in (
    "SCREEN_POWER",
    "draw_power_menu(",
    '"SLEEP"',
    '"RESTART"',
    '"POWER OFF"',
    '"RESTART FLIPOS?"',
    '"POWER OFF FLIPOS?"',
    "frontend_control_state.system.power_menu_requested",
    "flip_power_sleep_once()",
    "reboot(RB_AUTOBOOT)",
    "power_off_now()",
):
    assert token in frontend, token

assert "MENU+SELECT is intentionally game-owned and has no frontend action" in frontend

# In-game overlay is also fixed to 640x480 and uses a frozen game frame.
assert "flip_video_power_menu_view_t" in video_h
assert "flip_video_power_menu_begin(void)" in video
assert "flip_video_power_menu_present(" in video
assert "flip_video_power_menu_end(void)" in video

panel_x = define_int(video, "POWER_OVERLAY_PANEL_X")
panel_w = define_int(video, "POWER_OVERLAY_PANEL_W")
overlay_y = define_int(video, "POWER_OVERLAY_PANEL_Y")
overlay_h = define_int(video, "POWER_OVERLAY_PANEL_H")
overlay_row_y = define_int(video, "POWER_OVERLAY_ROW_Y")
overlay_row_h = define_int(video, "POWER_OVERLAY_ROW_H")
overlay_gap = define_int(video, "POWER_OVERLAY_ROW_GAP")

assert panel_x + panel_w <= 640
assert overlay_y + overlay_h <= 480
assert overlay_row_y + 3 * overlay_row_h + 2 * overlay_gap < overlay_y + overlay_h

# Destructive in-game actions must checkpoint before suspend/reboot/poweroff.
assert "FLIP_POWER_MENU_SLEEP" in host
assert "FLIP_POWER_MENU_RESTART" in host
assert "FLIP_POWER_MENU_OFF" in host
assert "lifecycle_save_bridge(" in host
assert '"SAVE FAILED - POWER ACTION CANCELLED"' in host
assert "flip_power_sleep_once()" in host
assert "reboot(RB_AUTOBOOT)" in host
assert "reboot(RB_POWER_OFF)" in host

print("Power Menu architecture tests passed")
