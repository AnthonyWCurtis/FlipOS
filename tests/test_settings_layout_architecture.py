#!/usr/bin/env python3
"""Geometry guardrails for the FlipOS Settings family."""

from pathlib import Path
import re

source = Path("src/frontend/flip_frontend.c").read_text()


def define_int(name: str) -> int:
    match = re.search(rf"^#define\s+{re.escape(name)}\s+(\d+)\s*$", source, re.M)
    assert match, f"missing layout define {name}"
    return int(match.group(1))


margin = define_int("SETTINGS_MARGIN_X")
content_w = define_int("SETTINGS_CONTENT_W")
list_y = define_int("SETTINGS_LIST_Y")
row_h = define_int("SETTINGS_ROW_H")
gap = define_int("SETTINGS_ROW_GAP")
footer = define_int("SETTINGS_FOOTER_Y")

assert content_w == 640 - 2 * margin
assert margin >= 16
assert footer + 7 <= 480

# Four System settings rows are the densest generic submenu.
assert list_y + 4 * row_h + 3 * gap < footer

settings_menu = source[
    source.index("static void draw_settings_menu("):
    source.index("static void draw_settings(")
]
settings = source[
    source.index("static void draw_settings("):
    source.index("static const char *bios_status_text(")
]

for block in (settings_menu, settings):
    assert "SETTINGS_MARGIN_X" in block
    assert "SETTINGS_CONTENT_W" in block
    assert "fit_text_to_width(" in block

for label in ("CONNECTIVITY", "SYSTEM", "INTERFACE"):
    assert label in settings

assert "ICON + PERCENTAGE" in settings_menu
assert "ICON ONLY" in settings_menu
assert "A SELECT   B BACK" in settings_menu
assert "A OPEN   B BACK" in settings

print("Settings layout architecture tests passed")
