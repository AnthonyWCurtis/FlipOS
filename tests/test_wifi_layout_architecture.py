#!/usr/bin/env python3
"""Geometry guardrails for the FlipOS Wi-Fi surfaces."""

from pathlib import Path
import re

source = Path("src/frontend/flip_frontend.c").read_text()


def define_int(name: str) -> int:
    match = re.search(rf"^#define\s+{re.escape(name)}\s+(\d+)\s*$", source, re.M)
    assert match, f"missing layout define {name}"
    return int(match.group(1))


margin = define_int("WIFI_MARGIN_X")
content_w = define_int("WIFI_CONTENT_W")
list_y = define_int("WIFI_LIST_Y")
row_h = define_int("WIFI_ROW_H")
gap = define_int("WIFI_ROW_GAP")
rows = define_int("WIFI_VISIBLE_ROWS")
message_y = define_int("WIFI_MESSAGE_Y")
footer_y = define_int("WIFI_FOOTER_Y")

password_y = define_int("WIFI_PASSWORD_CARD_Y")
password_h = define_int("WIFI_PASSWORD_CARD_H")
keyboard_y = define_int("WIFI_KEYBOARD_Y")
keyboard_gap = define_int("WIFI_KEYBOARD_ROW_GAP")
mode_y = define_int("WIFI_PASSWORD_MODE_Y")
password_footer = define_int("WIFI_PASSWORD_FOOTER_Y")

assert content_w == 640 - 2 * margin
assert margin >= 16
assert list_y + rows * row_h + (rows - 1) * gap < message_y
assert message_y + 7 < footer_y
assert footer_y + 14 <= 480

assert password_y + password_h < keyboard_y
# Current keyboard has at most 7 rows; leave room for status and mode text.
assert keyboard_y + 7 * keyboard_gap < mode_y + keyboard_gap
assert mode_y + 7 < password_footer
assert password_footer + 7 <= 480

wifi_start = source.index("static void draw_wifi(")
wifi_end = source.index("static void draw_wifi_password(", wifi_start)
wifi = source[wifi_start:wifi_end]

password_start = wifi_end
password_end = source.index("static int read_update_version(", password_start)
password = source[password_start:password_end]

for block in (wifi, password):
    assert "fit_text_to_width(" in block
    assert "WIFI_CONTENT_W" in block

assert "WIFI_VISIBLE_ROWS" in wifi
assert "draw_wifi_lock_icon(" in wifi
assert "CONNECTED:" in wifi
assert "NO NETWORKS FOUND" in wifi

assert "WIFI_PASSWORD_CARD_Y" in password
assert "wifi_keyboard.uppercase" in password
assert "wifi_keyboard.reveal_text" in password
assert "A TYPE   B DELETE/BACK   START CONNECT" in password

print("Wi-Fi layout architecture tests passed")
