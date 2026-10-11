from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
frontend = (ROOT / "src/frontend/flip_frontend.c").read_text()
setup = (ROOT / "src/runtime/flip_setup.c").read_text()
deploy = (ROOT / "tools/device/deploy-playtest").read_text()

for screen in (
    "SCREEN_FIRST_BOOT_WELCOME",
    "SCREEN_FIRST_BOOT_WIFI",
    "SCREEN_FIRST_BOOT_TIME",
    "SCREEN_FIRST_BOOT_PREFS",
    "SCREEN_FIRST_BOOT_BIOS",
    "SCREEN_FIRST_BOOT_READY",
):
    assert screen in frontend

assert "!flip_setup_is_complete()" in frontend
assert "flip_setup_begin()" in frontend
assert "flip_setup_mark_complete()" in frontend
assert "FLIP_DEV_FORCE_FIRST_BOOT" in frontend
assert "wifi_return_screen = SCREEN_FIRST_BOOT_WIFI" in frontend
assert "START CONTINUE" in frontend
assert "VOL -/+   VOLUME" in frontend
assert "MENU + VOL -/+   BRIGHTNESS" in frontend
assert '"BIOS FILES"' in frontend
assert '"OPTIONAL - USER SUPPLIED FILES"' in frontend
assert '"MISSING BIOS DOES NOT BLOCK SETUP"' in frontend
assert "flip_bios_query(" in frontend
assert "screen = SCREEN_FIRST_BOOT_BIOS" in frontend
assert "wifi_return_screen == SCREEN_FIRST_BOOT_WIFI" in frontend
assert "screen == SCREEN_WIFI &&" in frontend
assert "fsync(fd)" in setup
assert "rename(tmp, path)" in setup
assert "unlink(FLIP_SETUP_PROGRESS_FILE)" in setup
assert "state/setup/complete" in deploy

print("first-boot architecture tests passed")
assert '"A SET UP WI-FI   START SKIP"' in frontend
assert '"A WI-FI SETTINGS   START CONTINUE"' in frontend
assert '"A CONNECT   X RESCAN   START SKIP"' in frontend

assert '"DATE & TIME"' in frontend
assert '"LEFT/RIGHT ZONE   X SYNC   A CONTINUE"' in frontend
assert "flip_time_sync_ntp()" in frontend
assert "screen = SCREEN_FIRST_BOOT_TIME" in frontend
