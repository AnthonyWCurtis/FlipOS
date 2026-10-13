#!/usr/bin/env python3
"""Pixel-layout guardrails for Play Options / Save Safety launch UI."""

from pathlib import Path
import re

source = Path("src/frontend/flip_save_safety_launch.c").read_text()


def define_int(name: str) -> int:
    match = re.search(rf"^#define\s+{re.escape(name)}\s+(\d+)\s*$", source, re.M)
    assert match, name
    return int(match.group(1))


margin = define_int("PLAY_OPTIONS_MARGIN_X")
content = define_int("PLAY_OPTIONS_CONTENT_W")
row_y = define_int("PLAY_OPTIONS_ROW_Y")
row_h = define_int("PLAY_OPTIONS_ROW_H")
gap = define_int("PLAY_OPTIONS_ROW_GAP")
footer = define_int("PLAY_OPTIONS_FOOTER_Y")

assert content == 640 - 2 * margin
assert margin >= 16
assert row_y + 3 * row_h + 2 * gap < 386
assert 386 + 7 < footer
assert footer + 7 <= 480

start = source.index("static void draw_action_menu(")
end = source.index("static const char *undo_state_name(", start)
body = source[start:end]

for token in (
    '"PLAY OPTIONS"',
    '"CHOOSE HOW TO START"',
    '"RESUME GAME"',
    '"START WITHOUT RESUME"',
    '"SAVE SAFETY"',
    "PLAY_OPTIONS_ROW_Y",
    "PLAY_OPTIONS_ROW_H",
    '"A SELECT   B BACK"',
):
    assert token in body, token

assert "fit_text_to_width(" in source
assert "centred_text_x_in_rect(" in source

# Save Safety stays hidden for Beta; styling it must not enable it.
ui = Path("src/frontend/flip_save_safety_ui.c").read_text()
assert "#define FLIP_SAVE_SAFETY_USER_ENABLED 0" in ui

print("Play Options layout architecture tests passed")
