from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "src/frontend/flip_frontend.c").read_text()

for token in (
    "SCREEN_SETTINGS_CONNECTIVITY",
    "SCREEN_SETTINGS_SYSTEM",
    "SCREEN_SETTINGS_TIME",
    "SCREEN_SETTINGS_INTERFACE",
    '"CONNECTIVITY"',
    '"SYSTEM"',
    '"INTERFACE"',
    'static const char *connectivity_items[]',
    'static const char *system_settings_items[]',
    'static const char *interface_items[]',
    'wifi_return_screen = SCREEN_SETTINGS_CONNECTIVITY',
    'screen = SCREEN_SETTINGS_SYSTEM',
    'screen = SCREEN_SETTINGS_INTERFACE',
):
    assert token in source, token

# Wi-Fi page must not duplicate the global top-bar Wi-Fi icon.
wifi_start = source.index("static void draw_wifi(")
wifi_end = source.index("static void draw_wifi_password(", wifi_start)
wifi_draw = source[wifi_start:wifi_end]
assert "draw_wifi_status_icon" not in wifi_draw

# The keyboard labels must actually reflect the active case and the font must
# provide distinct lowercase glyphs.
assert "wifi_keyboard.uppercase &&" in source
assert "label[0] = (char)(key - 'a' + 'A')" in source
font = (ROOT / "src/hwtest/flip_draw.c").read_text()
assert "{'a'," in font
assert "{'z'," in font

# BIOS verifier is a System settings page, not a standalone top-level app.
assert '"BIOS VERIFIER"' in source
assert 'SCREEN_SETTINGS_BIOS' in source
assert 'draw_bios_verifier(' in source
assert 'FRONTEND_BIOS_VERIFIER entered' in source

print("settings navigation architecture tests passed")

assert '"DATE & TIME"' in source
assert 'draw_time_settings(' in source
assert 'FRONTEND_TIME_SETTINGS entered' in source
