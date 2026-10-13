#!/usr/bin/env python3
"""Pixel-layout guardrails for Date & Time."""

from pathlib import Path
import re

source = Path("src/frontend/flip_frontend.c").read_text()


def define_int(name: str) -> int:
    match = re.search(rf"^#define\s+{re.escape(name)}\s+(\d+)\s*$", source, re.M)
    assert match, name
    return int(match.group(1))


margin = define_int("SETTINGS_MARGIN_X")
content = define_int("SETTINGS_CONTENT_W")
list_y = define_int("SETTINGS_LIST_Y")
row_h = define_int("SETTINGS_ROW_H")
gap = define_int("SETTINGS_ROW_GAP")
footer = define_int("SETTINGS_FOOTER_Y")

assert content == 640 - 2 * margin
assert list_y + 4 * row_h + 3 * gap < 410
assert 410 + 7 < footer
assert footer + 14 <= 480

start = source.index("static void draw_time_settings(")
end = source.index("static void draw_settings_menu(", start)
body = source[start:end]

for token in (
    "SETTINGS_MARGIN_X",
    "SETTINGS_CONTENT_W",
    "SETTINGS_LIST_Y",
    "fit_text_to_width(",
    "ui_text_width_px(",
    '"AUTOMATIC TIME   %s"',
    '"SYNC TIME NOW"',
    '"SET DATE & TIME MANUALLY"',
    '"A SAVE   B CANCEL"',
):
    assert token in body, token

assert "flip_draw_rect(" in body
assert "time_manual_editing" in body

print("Date & Time layout architecture tests passed")
