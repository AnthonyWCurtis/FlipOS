from pathlib import Path
import string

ROOT = Path(__file__).resolve().parents[1]
frontend = (ROOT / "src/frontend/flip_frontend.c").read_text()
draw = (ROOT / "src/hwtest/flip_draw.c").read_text()
keyboard = (ROOT / "src/runtime/flip_keyboard.c").read_text()

assert "wifi_keyboard.uppercase" in frontend
assert "key - 'a' + 'A'" in frontend
assert "keyboard->uppercase" in keyboard
for ch in "abcdefghijklmnopqrstuvwxyz":
    assert "{'%s'," % ch in draw, ch

for ch in string.punctuation:
    if ch == "'":
        needle = "{'\\'',"
    elif ch == "\\":
        needle = "{'\\\\',"
    else:
        needle = "{'%s'," % ch
    assert needle in draw, f"missing drawable keyboard glyph: {ch!r}"
print("keyboard case architecture tests passed")
