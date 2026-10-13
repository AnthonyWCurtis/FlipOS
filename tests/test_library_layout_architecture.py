#!/usr/bin/env python3
"""Geometry guardrails for the fixed 640x480 FlipOS Library surfaces."""

from pathlib import Path
import re

source = Path("src/frontend/flip_frontend.c").read_text()


def define_int(name: str) -> int:
    match = re.search(rf"^#define\s+{re.escape(name)}\s+(\d+)\s*$", source, re.M)
    assert match, f"missing integer layout define {name}"
    return int(match.group(1))


W = define_int("LIBRARY_SCREEN_W")
H = define_int("LIBRARY_SCREEN_H")
margin = define_int("LIBRARY_MARGIN_X")
content_w = define_int("LIBRARY_CONTENT_W")
list_y = define_int("LIBRARY_LIST_Y")
list_bottom = define_int("LIBRARY_LIST_BOTTOM")
system_h = define_int("LIBRARY_SYSTEM_ROW_H")
system_gap = define_int("LIBRARY_SYSTEM_GAP")
game_h = define_int("LIBRARY_GAME_ROW_H")
game_gap = define_int("LIBRARY_GAME_GAP")
footer_y = define_int("LIBRARY_FOOTER_Y")

assert (W, H) == (640, 480)
assert content_w == W - 2 * margin
assert margin >= 16
assert list_y >= 64
assert list_bottom < footer_y
assert footer_y + 7 <= H

system_rows = (list_bottom - list_y + system_gap) // (system_h + system_gap)
game_rows = (list_bottom - list_y + game_gap) // (game_h + game_gap)

assert system_rows >= 4
assert game_rows >= 6
assert list_y + (system_rows - 1) * (system_h + system_gap) + system_h <= list_bottom
assert list_y + (game_rows - 1) * (game_h + game_gap) + game_h <= list_bottom

systems = source[
    source.index("static void draw_systems("):
    source.index("static void draw_games(")
]
games = source[
    source.index("static void draw_games("):
    source.index("static void draw_recent(")
]

for block in (systems, games):
    assert "fit_text_to_width(" in block
    assert "LIBRARY_CONTENT_W" in block

assert "HOLD MENU + SELECT TO POWER OFF" not in systems
assert "HOLD MENU + SELECT TO POWER OFF" not in games
assert "LIBRARY_GAME_ROW_H" in games
assert "LIBRARY_SYSTEM_ROW_H" in systems

def width(text: str, scale: int) -> int:
    return len(text) * 6 * scale

assert width("A LAUNCH   Y DETAILS   B BACK", 1) <= content_w
assert width("A OPEN   B BACK", 1) <= content_w

print("Library layout architecture tests passed")
