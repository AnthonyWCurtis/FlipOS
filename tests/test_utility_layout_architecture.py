#!/usr/bin/env python3
"""Geometry guardrails for FlipOS utility screens."""

from pathlib import Path
import re

source = Path("src/frontend/flip_frontend.c").read_text()


def define_int(name: str) -> int:
    match = re.search(rf"^#define\s+{re.escape(name)}\s+(\d+)\s*$", source, re.M)
    assert match, f"missing layout define {name}"
    return int(match.group(1))


margin = define_int("UTILITY_MARGIN_X")
content_w = define_int("UTILITY_CONTENT_W")
panel_y = define_int("UTILITY_PANEL_Y")
panel_h = define_int("UTILITY_PANEL_H")
message_y = define_int("UTILITY_MESSAGE_Y")
footer_y = define_int("UTILITY_FOOTER_Y")

bios_y = define_int("BIOS_ROW_Y")
bios_h = define_int("BIOS_ROW_H")
bios_gap = define_int("BIOS_ROW_GAP")
bios_hint = define_int("BIOS_HINT_Y")

assert content_w == 640 - 2 * margin
assert panel_y >= 60
assert panel_y + panel_h < message_y
assert message_y + 7 < footer_y
assert footer_y + 7 <= 480

assert bios_y + 3 * bios_h + 2 * bios_gap < bios_hint
assert bios_hint + 7 < footer_y

update_start = source.index("static void draw_update(")
update_end = source.index("static void refresh_time_status(", update_start)
update = source[update_start:update_end]

bios_start = source.index("static void draw_bios_verifier(")
bios_end = source.index("static void draw_optimizer_results(", bios_start)
bios = source[bios_start:bios_end]

for block in (update, bios):
    assert "UTILITY_MARGIN_X" in block
    assert "UTILITY_CONTENT_W" in block
    assert "UI_SCALE_" in block

assert "fit_text_to_width(" in update
assert "A INSTALL   X CHECK GITHUB   Y DELETE" in update
assert "A RESTART NOW   B LATER" in update
assert "A CHECK GITHUB" in update

assert "BIOS_ROW_H" in bios
assert "fit_text_to_width(" in bios
assert "ui_text_width_px(" in bios
assert "VALID = SIZE + SHA-256 MATCH" in bios
assert "A RECHECK   B BACK" in bios

print("utility layout architecture tests passed")
