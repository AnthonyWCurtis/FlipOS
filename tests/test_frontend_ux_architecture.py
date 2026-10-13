from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src/frontend/flip_frontend.c").read_text()
DRAW_SOURCE = (ROOT / "src/hwtest/flip_draw.c").read_text()

assert '"HARDWARE TEST"' not in SOURCE
assert '"COMING SOON"' not in SOURCE
assert '"NO GB ROMS FOUND"' not in SOURCE
assert '"PUT ROMS IN ROMS GB"' not in SOURCE
assert '"ADD GAMES TO THE SD CARD"' in SOURCE
assert '"VOLUME %d%%   BRIGHTNESS %d%%"' in SOURCE
assert '? "PAUSED"' in SOURCE
assert 'centred_text_x("LAUNCHING", 3)' in SOURCE

assert '"HOLD MENU + SELECT TO POWER OFF"' not in SOURCE
assert "POWEROFF_HOLD_SECONDS" not in SOURCE
assert '"YOUR GAMES. YOUR TIME."' in SOURCE
assert '"LAST PLAYED"' in SOURCE
assert '"PLAY STATS"' in SOURCE
assert '"PLAY TIME"' in SOURCE
assert '"LAUNCHES"' in SOURCE
assert 'SCREEN_STATS' in SOURCE
assert 'draw_stats(' in SOURCE
assert 'SCREEN_GAME_DETAILS' in SOURCE
assert 'draw_game_details(' in SOURCE
assert '"A LAUNCH   Y DETAILS   B BACK"' in SOURCE
assert '"END PAUSED GAME?"' in SOURCE
assert '"REMOVE FROM LIST?"' in SOURCE
assert '"RUN STABILITY TEST"' in SOURCE
assert '"RESTORE DEFAULT SETTINGS"' in SOURCE
assert '"PROFILE QUARANTINED"' not in SOURCE
assert '"VALIDATION FAILED"' not in SOURCE
assert '"DEVICE-SPECIFIC CPU VOLTAGE CALIBRATION"' not in SOURCE
assert '"MENU + VOL -/+   BRIGHTNESS"' in SOURCE

safety = (ROOT / "src/frontend/flip_save_safety_launch.c").read_text()
assert '"PLAY OPTIONS"' in safety
assert '"START WITHOUT RESUME"' in safety
assert '"UNDO RESTORE"' in safety

assert 'static void draw_battery_indicator(flip_drm_t *drm)' in SOURCE
assert 'flip_battery_read(&battery)' in SOURCE
assert 'FLIP_BATTERY_STATUS_CHARGING' in SOURCE
assert 'battery_now >= next_battery_refresh' in SOURCE
assert SOURCE.count('draw_battery_indicator(') >= 8

assert 'SCREEN_WIFI' in SOURCE
assert '"WI-FI"' in SOURCE
assert 'refresh_wifi_networks()' in SOURCE
assert 'flip_wifi_scan(' in SOURCE

assert 'SCREEN_WIFI_PASSWORD' in SOURCE
assert 'draw_wifi_password(' in SOURCE
assert 'START CONNECT' in SOURCE
assert 'flip_wifi_connect(' in SOURCE
assert 'clear_wifi_password()' in SOURCE

assert 'Y %s   X %s PASSWORD' in SOURCE

assert 'FLIP_KEYBOARD_COLUMNS' in SOURCE
assert 'flip_keyboard_key_count()' in SOURCE
assert 'WIFI_KEYBOARD_Y' in SOURCE
assert 'WIFI_KEYBOARD_ROW_GAP' in SOURCE
assert 'col * 52' in SOURCE

assert 'CONNECTED - NO NETWORK ADDRESS' in SOURCE

assert 'wifi_status.usable_ip' in SOURCE

assert 'flipos-dev-suppress-low-battery-poweroff' in SOURCE

assert 'Y FORGET' in SOURCE

assert 'flip_wifi_forget_network(selected_ssid)' in SOURCE
assert 'flip_wifi_connect_saved(network->ssid)' in SOURCE
assert 'if (network->saved)' in SOURCE
assert 'flip_wifi_network_is_saved(network->ssid)' not in SOURCE

assert 'SAVED NETWORK FORGOTTEN' in SOURCE

assert '#include "flip_keyboard.h"' in SOURCE
assert 'flip_keyboard_init(&wifi_keyboard)' in SOURCE
assert 'flip_keyboard_toggle_case(&wifi_keyboard)' in SOURCE
assert 'flip_keyboard_toggle_reveal(&wifi_keyboard)' in SOURCE
assert 'SCREEN_FIRST_BOOT_WELCOME' in SOURCE
assert 'SCREEN_FIRST_BOOT_WIFI' in SOURCE
assert 'SCREEN_FIRST_BOOT_PREFS' in SOURCE
assert 'SCREEN_FIRST_BOOT_READY' in SOURCE
assert 'flip_setup_mark_complete()' in SOURCE
assert 'WI-FI IS OPTIONAL' in SOURCE

assert '"SYSTEM UPDATE"' in SOURCE
assert 'SCREEN_UPDATE' in SOURCE
assert 'run_update_installer()' in SOURCE
assert 'UPDATE FAILED - CURRENT VERSION KEPT' in SOURCE
assert 'UPDATE VERIFIED AND INSTALLED' in SOURCE
assert 'CHECK GITHUB FOR THE LATEST FLIPOS RELEASE' in SOURCE

assert 'wifi_signal_level(' in SOURCE
assert 'draw_wifi_signal_icon(' not in SOURCE
assert 'draw_wifi_lock_icon(' in SOURCE
assert 'CONNECTED - WAITING FOR NETWORK ADDRESS' in SOURCE

assert 'update_confirm' in SOURCE
assert 'A CONFIRM   B CANCEL' in SOURCE
assert 'CURRENT VERSION IS KEPT IF INSTALL FAILS' in SOURCE

assert 'const int icon_w = 23' in SOURCE



assert 'const int icon_h = 13' in SOURCE

assert 'draw_topbar_wifi_indicator(' in SOURCE
assert 'flip_settings_get_battery_percent_visible() ? 552 : 581' in SOURCE
assert 'draw_topbar_wifi_indicator(drm);' in SOURCE
assert '(void)flip_wifi_get_status(&wifi_status);' in SOURCE

assert 'wifi_topbar_signal_level' in SOURCE
assert 'refresh_topbar_wifi_signal(' in SOURCE
assert 'i <= level && wifi_status.usable_ip' in SOURCE
assert 'draw_wifi_signal_icon(\n            drm,\n            76' not in SOURCE

assert '"BATTERY INDICATOR"' in SOURCE
assert '"ICON + PERCENTAGE"' in SOURCE
assert '"ICON ONLY"' in SOURCE
assert 'flip_settings_get_battery_percent_visible()' in SOURCE
assert 'flip_settings_set_battery_percent_visible(!visible)' in SOURCE
assert 'icon_x + icon_w + 7' in SOURCE
assert 'int fill_w = (inner_w * percent + 99) / 100' in SOURCE

draw_source = (ROOT / 'src/hwtest/flip_draw.c').read_text()
assert "{'a'," in draw_source
assert "{'z'," in draw_source
assert "c -= ('a' - 'A')" not in draw_source
assert "wifi_keyboard.uppercase &&" in SOURCE
assert "label[0] = (char)(key - 'a' + 'A')" in SOURCE

assert 'draw_wifi_status_icon(drm, 52, 35);' not in SOURCE

assert 'flip_settings_get_battery_percent_visible() ? 582 : 611' in SOURCE

assert '"CONNECTIVITY"' in SOURCE
assert '"SYSTEM"' in SOURCE
assert 'draw_wifi_status_icon' not in SOURCE
assert "{'a'," in DRAW_SOURCE
assert "{'z'," in DRAW_SOURCE

assert 'SCREEN_SETTINGS_CONNECTIVITY' in SOURCE
assert 'SCREEN_SETTINGS_SYSTEM' in SOURCE
assert 'SCREEN_SETTINGS_INTERFACE' in SOURCE
assert '"CONNECTIVITY"' in SOURCE
assert '"SYSTEM"' in SOURCE
assert '"INTERFACE"' in SOURCE
assert 'wifi_return_screen = SCREEN_SETTINGS_CONNECTIVITY' in SOURCE
assert 'screen = SCREEN_SETTINGS_SYSTEM' in SOURCE
assert 'screen = SCREEN_SETTINGS_INTERFACE' in SOURCE
assert 'draw_settings_menu(' in SOURCE

assert 'FLIP_UPDATE_FETCHER' in SOURCE
assert 'run_update_fetcher()' in SOURCE
assert 'A CHECK GITHUB' in SOURCE
assert 'X CHECK GITHUB' in SOURCE
assert 'UPDATE DOWNLOADED - READY TO INSTALL' in SOURCE
assert "YOU'RE ON THE LATEST RELEASE" in SOURCE
assert 'access(FLIP_UPDATE_AVAILABLE, F_OK) != 0' in SOURCE
assert 'CHECK GITHUB FOR THE LATEST FLIPOS RELEASE' in SOURCE

print("frontend UX architecture tests passed")
