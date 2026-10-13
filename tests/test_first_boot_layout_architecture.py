#!/usr/bin/env python3
"""Pixel-layout guardrails for FlipOS first boot."""

from pathlib import Path
import re

source = Path("src/frontend/flip_frontend.c").read_text()


def define_int(name: str) -> int:
    match = re.search(rf"^#define\s+{re.escape(name)}\s+(\d+)\s*$", source, re.M)
    assert match, name
    return int(match.group(1))


margin = define_int("FIRST_BOOT_MARGIN_X")
content = define_int("FIRST_BOOT_CONTENT_W")
panel_y = define_int("FIRST_BOOT_PANEL_Y")
panel_h = define_int("FIRST_BOOT_PANEL_H")
footer = define_int("FIRST_BOOT_FOOTER_Y")

assert content == 640 - 2 * margin
assert panel_y >= 100
assert panel_y + panel_h < 362
assert footer + 14 <= 480

start = source.index("static void draw_first_boot(")
end = source.index("static void refresh_topbar_wifi_signal(void);", start)
body = source[start:end]

for token in (
    "FIRST_BOOT_MARGIN_X",
    "FIRST_BOOT_CONTENT_W",
    "fit_text_to_width(",
    "ui_text_width_px(",
    '"WI-FI IS OPTIONAL"',
    '"DATE & TIME"',
    '"CONTROLS & DISPLAY"',
    '"BIOS FILES"',
    '"READY"',
    '"A START FLIPOS"',
):
    assert token in body, token

# Dynamic SSID/time/status strings must be bounded inside the fixed panel.
assert "wifi_visible" in body
assert "now_visible" in body
assert "values_visible" in body

print("first boot layout architecture tests passed")
