#!/usr/bin/env python3
"""Geometry guardrails for the fixed 640x480 FlipOS GameSwitcher."""

from pathlib import Path
import re

source = Path("src/frontend/flip_frontend.c").read_text()


def define_int(name: str) -> int:
    match = re.search(rf"^#define\s+{re.escape(name)}\s+(\d+)\s*$", source, re.M)
    assert match, f"missing integer layout define {name}"
    return int(match.group(1))


W = define_int("SWITCHER_SCREEN_W")
H = define_int("SWITCHER_SCREEN_H")
margin = define_int("SWITCHER_MARGIN_X")
content_w = define_int("SWITCHER_CONTENT_W")

hero_x = define_int("SWITCHER_HERO_X")
hero_y = define_int("SWITCHER_HERO_Y")
hero_w = define_int("SWITCHER_HERO_W")
hero_h = define_int("SWITCHER_HERO_H")

strip_label_y = define_int("SWITCHER_STRIP_LABEL_Y")
card_y = define_int("SWITCHER_CARD_Y")
card_h = define_int("SWITCHER_CARD_H")
card_w = define_int("SWITCHER_CARD_W")
card_gap = define_int("SWITCHER_CARD_GAP")
footer_y = define_int("SWITCHER_FOOTER_Y")

assert (W, H) == (640, 480)
assert content_w == W - 2 * margin

assert hero_x >= margin
assert hero_x + hero_w <= W - margin
assert hero_y >= 40
assert hero_y + hero_h < strip_label_y

cards_right = margin + 3 * card_w + 2 * card_gap
assert cards_right <= W - margin
assert card_y >= strip_label_y
assert card_y + card_h < footer_y

# Footer is fully visible using the fixed 5x7 font.
assert footer_y + 7 <= H

# Three cards use their own bounded title regions; the longest fixed footer
# must fit the 592 px safe width at scale 1.
def text_width(text: str, scale: int) -> int:
    return len(text) * 6 * scale

for help_text in (
    "A RESUME   X END   Y DETAILS   B BACK",
    "A RESUME   X REMOVE   Y DETAILS   B BACK",
    "A LAUNCH   X REMOVE   Y DETAILS   B BACK",
):
    assert text_width(help_text, 1) <= content_w, help_text

switcher = source[
    source.index("static void draw_recent("):
    source.index("static int launch_game_once(")
]

assert "SWITCHER_HERO_X" in switcher
assert "SWITCHER_CARD_Y" in switcher
assert "fit_text_to_width(" in switcher
assert "ui_text_width_px(" in switcher
assert "RECENT SESSIONS" in switcher
assert "ROWS_VISIBLE" not in switcher

print("GameSwitcher layout architecture tests passed")
