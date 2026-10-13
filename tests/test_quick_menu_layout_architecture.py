#!/usr/bin/env python3
"""Pixel-layout guardrails for the in-game Quick Menu."""

from pathlib import Path
import re

source = Path("src/runtime/flip_video.c").read_text()


def define_int(name: str) -> int:
    match = re.search(rf"^#define\s+{re.escape(name)}\s+(\d+)\s*$", source, re.M)
    assert match, name
    return int(match.group(1))


screen_w = define_int("QUICK_MENU_SCREEN_W")
screen_h = define_int("QUICK_MENU_SCREEN_H")
panel_x = define_int("QUICK_MENU_PANEL_X")
panel_y = define_int("QUICK_MENU_PANEL_Y")
panel_w = define_int("QUICK_MENU_PANEL_W")
panel_h = define_int("QUICK_MENU_PANEL_H")
inner_x = define_int("QUICK_MENU_INNER_X")
inner_w = define_int("QUICK_MENU_INNER_W")
item_y = define_int("QUICK_MENU_ITEM_Y")
item_h = define_int("QUICK_MENU_ITEM_H")
item_gap = define_int("QUICK_MENU_ITEM_GAP")
status_y = define_int("QUICK_MENU_STATUS_Y")
footer_y = define_int("QUICK_MENU_FOOTER_Y")

assert (screen_w, screen_h) == (640, 480)
assert panel_x >= 24
assert panel_y >= 20
assert panel_x + panel_w <= screen_w - 24
assert panel_y + panel_h <= screen_h - 20

assert inner_x >= panel_x
assert inner_x + inner_w <= panel_x + panel_w

items_bottom = item_y + 8 * item_h + 7 * item_gap
assert items_bottom < status_y
assert status_y + 7 < footer_y
assert footer_y + 7 <= panel_y + panel_h

start = source.index("int flip_video_quick_menu_present(")
end = source.index("void flip_video_quick_menu_end(", start)
body = source[start:end]

for label in (
    '"RESUME"',
    '"SAVE STATE"',
    '"LOAD STATE"',
    '"VOLUME"',
    '"BRIGHTNESS"',
    '"PERFORMANCE HUD"',
    '"GAMESWITCHER"',
    '"QUIT GAME"',
):
    assert label in body, label

assert "QUICK_MENU_INNER_W" in body
assert "selected ? 2 : 1" in body
assert '"A SELECT   B OR MENU RESUME   LEFT RIGHT ADJUST"' in body
assert "quick_menu_restore_to_drm();" in body
assert "(row[x] >> 2) &" in body
assert "0x003f3f3f" in body
assert "video_gpu_active" in body
assert "video_overlay_pixels" in body
assert "gpu_menu_submit_rect(" in body

print("Quick Menu layout architecture tests passed")
