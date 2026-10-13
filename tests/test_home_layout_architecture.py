#!/usr/bin/env python3
"""Geometry guardrails for the fixed 640x480 FlipOS Home screen."""

from pathlib import Path
import re

source = Path("src/frontend/flip_frontend.c").read_text()


def define_int(name: str) -> int:
    match = re.search(rf"^#define\s+{re.escape(name)}\s+(\d+)\s*$", source, re.M)
    assert match, f"missing integer layout define {name}"
    return int(match.group(1))


W = define_int("HOME_SCREEN_W")
H = define_int("HOME_SCREEN_H")
margin = define_int("HOME_MARGIN_X")
content_w = define_int("HOME_CONTENT_W")

hero_x = define_int("HOME_HERO_X")
hero_y = define_int("HOME_HERO_Y")
hero_w = define_int("HOME_HERO_W")
hero_h = define_int("HOME_HERO_H")

recent_label_y = define_int("HOME_RECENT_LABEL_Y")
recent_y = define_int("HOME_RECENT_Y")
recent_h = define_int("HOME_RECENT_H")
recent_w = define_int("HOME_RECENT_CARD_W")
recent_gap = define_int("HOME_RECENT_GAP")

nav_y = define_int("HOME_NAV_Y")
nav_h = define_int("HOME_NAV_H")
nav_w = define_int("HOME_NAV_W")
nav_gap = define_int("HOME_NAV_GAP")

message_y = define_int("HOME_MESSAGE_Y")
footer_y = define_int("HOME_FOOTER_Y")

assert (W, H) == (640, 480)
assert margin >= 16
assert content_w == W - 2 * margin

# Hero is fully on-screen and leaves the hardware-status top bar untouched.
assert hero_x >= margin
assert hero_x + hero_w <= W - margin
assert hero_y >= 40
assert hero_y + hero_h <= recent_label_y

# Three recent cards exactly fit the same safe content width.
recent_right = margin + 3 * recent_w + 2 * recent_gap
assert recent_right <= W - margin
assert recent_y >= recent_label_y
assert recent_y + recent_h < nav_y

# Four secondary navigation cells fit without crossing each other or the edge.
nav_right = margin + 4 * nav_w + 3 * nav_gap
assert nav_right <= W - margin
assert nav_y + nav_h <= message_y

# Message and footer have distinct vertical bands.
assert message_y + 7 < footer_y
assert footer_y + 7 <= H

# 5x7 font advances 6 px per character per scale.
def text_width(text: str, scale: int) -> int:
    return len(text) * 6 * scale

for label in ("LIBRARY", "SWITCHER", "PLAY STATS", "SETTINGS"):
    assert text_width(label, 2) <= nav_w - 12, label

for footer in ("A PLAY   DOWN MORE", "A OPEN   UP CONTINUE   LEFT RIGHT"):
    assert text_width(footer, 1) <= content_w, footer

# Home must use measured/clamped text rather than unchecked long labels.
assert "static int ui_text_width_px(" in source
assert "static void fit_text_to_width(" in source
assert "centred_text_x_in_rect(" in source

# The old left sidebar must not creep back into Home.
home = source[source.index("static void draw_home("):source.index("static void draw_stats(")]
assert "menu_x" not in home
assert "YOUR GAMES. YOUR TIME." in home
assert "HOME_HERO_X" in home
assert "HOME_NAV_Y" in home

# CONTINUE is a real launch path, not decorative copy.
assert '"CONTINUE",' in source
assert "FRONTEND_HOME lifecycle failed" in source

print("home layout architecture tests passed")
