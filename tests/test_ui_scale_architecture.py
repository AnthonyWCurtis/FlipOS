from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "src/frontend/flip_frontend.c").read_text()

for token in (
    "#define UI_SCALE_HERO      5",
    "#define UI_SCALE_TITLE     4",
    "#define UI_SCALE_SECTION   3",
    "#define UI_SCALE_BODY      2",
    "#define UI_SCALE_META      2",
    "#define UI_SCALE_HINT      2",
    "#define UI_FOOTER_Y      448",
):
    assert token in source, token

for function in (
    "draw_first_boot",
    "draw_home",
    "draw_systems",
    "draw_games",
    "draw_recent",
    "draw_settings",
    "draw_settings_menu",
    "draw_wifi",
    "draw_wifi_password",
    "draw_bios_verifier",
    "draw_update",
    "draw_optimizer",
    "draw_optimizer_results",
):
    start = source.index("static void " + function + "(")
    end = source.find("\nstatic ", start + 20)
    body = source[start:end if end >= 0 else len(source)]
    assert "UI_SCALE_" in body, function

# Onboarding copy must remain readable at the shared body scale without
# overflowing the 640px panel.
assert '"BIOS VERIFIER IS IN SETTINGS > SYSTEM"' in source
assert '"COPY THEM LATER FROM SETTINGS > SYSTEM > BIOS VERIFIER"' not in source

print("ui scale architecture tests passed")


# Interface chrome is centered; scan-oriented data rows remain free to use
# structured alignment.
for token in (
    "centred_text_x(title, UI_SCALE_HERO)",
    "centred_text_x(line1, UI_SCALE_BODY)",
    "centred_text_x(line2, UI_SCALE_HINT)",
    "centred_text_x(wifi_footer, UI_SCALE_HINT)",
):
    assert token in source, token


def split_call_args(body):
    args = []
    last = 0
    depth = 0
    in_string = False
    escaped = False
    for index, ch in enumerate(body):
        if in_string:
            if escaped:
                escaped = False
            elif ch == "\\":
                escaped = True
            elif ch == '"':
                in_string = False
            continue
        if ch == '"':
            in_string = True
        elif ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
        elif ch == "," and depth == 0:
            args.append(body[last:index].strip())
            last = index + 1
    args.append(body[last:].strip())
    return args

def function_calls(source, name):
    needle = name + "("
    pos = 0
    while True:
        start = source.find(needle, pos)
        if start < 0:
            return
        cursor = start + len(needle)
        depth = 1
        in_string = False
        escaped = False
        while cursor < len(source) and depth:
            ch = source[cursor]
            if in_string:
                if escaped:
                    escaped = False
                elif ch == "\\":
                    escaped = True
                elif ch == '"':
                    in_string = False
            else:
                if ch == '"':
                    in_string = True
                elif ch == "(":
                    depth += 1
                elif ch == ")":
                    depth -= 1
            cursor += 1
        yield split_call_args(source[start + len(needle):cursor - 1])
        pos = cursor

for args in function_calls(source, "flip_draw_text"):
    if len(args) < 5 or not args[1].startswith("centred_text_x("):
        continue
    centered = args[1][len("centred_text_x("):-1]
    centered_args = split_call_args(centered)
    assert len(centered_args) == 2
    center_scale = centered_args[1]
    draw_scale = args[4]
    assert center_scale == draw_scale, (
        f"centering scale drift: {args[3]} uses {center_scale} "
        f"to center but {draw_scale} to draw"
    )


# Interface Settings has a two-line selected row. Its highlight must be taller
# than the standard single-line Settings row so the mode text cannot clip.
settings_menu_start = source.index("static void draw_settings_menu(")
settings_menu_end = source.index("\nstatic void draw_settings(", settings_menu_start)
settings_menu = source[settings_menu_start:settings_menu_end]
assert "(show_battery_mode && i == 0) ? 72 : 54" in settings_menu
assert "selection_height" in settings_menu


# Library browsing is a first-class surface: Games/System/GameSwitcher primary
# rows must use the same section scale as the rest of FlipOS, not the old dense
# scale-2 browser.
assert "#define ROWS_VISIBLE 7" in source
games_start = source.index("static void draw_games(")
games_end = source.index("\nstatic void draw_recent(", games_start)
games = source[games_start:games_end]
assert "visible,\n                UI_SCALE_SECTION" in games
assert "92 + row * 44" in games
recent_start = source.index("static void draw_recent(")
recent_end = source.index("\nstatic int launch_game_once(", recent_start)
recent = source[recent_start:recent_end]
assert "visible,\n                UI_SCALE_SECTION" in recent
assert "status,\n                    UI_SCALE_META" in recent
assert "92 + row * 44" in recent


# Top-left status: normal state shows VERSION; a validated newer package swaps
# that label for the update/download arrow.
assert "draw_topbar_version_indicator" in source
assert "FLIP_VERSION_FILE" in source
assert "FLIP_UPDATE_AVAILABLE" in source
assert "access(FLIP_UPDATE_AVAILABLE, F_OK) == 0" in source
