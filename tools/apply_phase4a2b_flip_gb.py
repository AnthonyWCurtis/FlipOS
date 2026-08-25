#!/usr/bin/env python3

from pathlib import Path
import re
import sys

PATH = Path("src/emulators/flip_gb.c")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(
            f"{label}: expected exactly one match, found {count}"
        )
    return text.replace(old, new, 1)


def regex_replace_once(
    text: str,
    pattern: str,
    replacement: str,
    label: str,
) -> str:
    updated, count = re.subn(
        pattern,
        replacement,
        text,
        count=1,
        flags=re.DOTALL,
    )
    if count != 1:
        raise RuntimeError(
            f"{label}: expected exactly one match, found {count}"
        )
    return updated


def main() -> int:
    if not PATH.exists():
        print(f"missing {PATH}", file=sys.stderr)
        return 1

    text = PATH.read_text()

    if '#include "flip_controls.h"' in text:
        print("flip_gb.c already contains Phase 4A2B controls migration")
        return 0

    text = replace_once(
        text,
        '#include "flip_input.h"\n#include "flip_hotkeys.h"',
        '#include "flip_input.h"\n#include "flip_controls.h"\n#include "flip_hotkeys.h"',
        "controls include",
    )

    text = replace_once(
        text,
        'static flip_input_state_t input_state;\n',
        'static flip_input_state_t physical_input;\n'
        'static flip_controls_t controls;\n'
        'static flip_controls_state_t control_state;\n',
        "input state declaration",
    )

    text = replace_once(
        text,
        'static int fps_overlay_enabled = 0;\n'
        'static double fps_overlay_value = 0.0;\n'
        'static int previous_r1 = 0;\n'
        'static int previous_menu = 0;\n'
        'static int menu_pending = 0;\n\n'
        'static flip_hotkeys_t global_hotkeys;\n\n'
        'static int previous_lid_ready = 0;\n'
        'static int previous_lid_closed = 0;\n',
        'static int fps_overlay_enabled = 0;\n'
        'static double fps_overlay_value = 0.0;\n',
        "legacy shortcut state",
    )

    mappings = {
        'input_state.down[\n                FLIP_BTN_UP]':
            'control_state.game.down[\n                FLIP_GAME_UP]',
        'input_state.down[\n                FLIP_BTN_DOWN]':
            'control_state.game.down[\n                FLIP_GAME_DOWN]',
        'input_state.down[\n                FLIP_BTN_LEFT]':
            'control_state.game.down[\n                FLIP_GAME_LEFT]',
        'input_state.down[\n                FLIP_BTN_RIGHT]':
            'control_state.game.down[\n                FLIP_GAME_RIGHT]',
        'input_state.down[\n                FLIP_BTN_A]':
            'control_state.game.down[\n                FLIP_GAME_A]',
        'input_state.down[\n                FLIP_BTN_B]':
            'control_state.game.down[\n                FLIP_GAME_B]',
        'input_state.down[\n                FLIP_BTN_START]':
            'control_state.game.down[\n                FLIP_GAME_START]',
        'input_state.down[\n                FLIP_BTN_SELECT]':
            'control_state.game.down[\n                FLIP_GAME_SELECT]',
    }

    for old, new in mappings.items():
        text = replace_once(
            text,
            old,
            new,
            f"game mapping {old.split('FLIP_BTN_')[-1].split(']')[0]}",
        )

    text = replace_once(
        text,
        '    memset(\n'
        '        &input_state,\n'
        '        0,\n'
        '        sizeof(input_state));\n',
        '    memset(\n'
        '        &physical_input,\n'
        '        0,\n'
        '        sizeof(physical_input));\n\n'
        '    memset(\n'
        '        &control_state,\n'
        '        0,\n'
        '        sizeof(control_state));\n\n'
        '    flip_controls_init(\n'
        '        &controls);\n',
        "input initialization",
    )

    text = replace_once(
        text,
        '    flip_hotkeys_init(\n'
        '        &global_hotkeys);\n\n',
        '',
        "legacy hotkey state initialization",
    )

    text = replace_once(
        text,
        '        if (flip_input_poll(\n'
        '                &input_state,\n'
        '                0) < 0) {\n\n'
        '            fprintf(\n'
        '                stderr,\n'
        '                "FLIPGB: input poll failed\\n");\n\n'
        '            break;\n'
        '        }\n\n'
        '        retro_run();',
        '        if (flip_input_poll(\n'
        '                &physical_input,\n'
        '                0) < 0) {\n\n'
        '            fprintf(\n'
        '                stderr,\n'
        '                "FLIPGB: input poll failed\\n");\n\n'
        '            break;\n'
        '        }\n\n'
        '        if (flip_controls_update(\n'
        '                &controls,\n'
        '                &physical_input,\n'
        '                &control_state) < 0) {\n\n'
        '            fprintf(\n'
        '                stderr,\n'
        '                "FLIPGB: controls update failed\\n");\n\n'
        '            break;\n'
        '        }\n\n'
        '        retro_run();',
        "per-frame controls transform",
    )

    actions_block = r'''        /*
         * FlipOS logical system actions.
         *
         * Physical button/chord interpretation belongs to
         * flip_controls. Flip_GB only reacts to the resulting
         * platform-neutral host actions.
         */
        if (control_state.system.lid_changed) {

            fprintf(
                stderr,
                "FLIPGB_LID %s\n",
                control_state.system.lid_closed
                    ? "CLOSED"
                    : "OPEN");
        }

        if (control_state.system.suspend_requested) {

            fprintf(
                stderr,
                "FLIPGB_LID suspend requested\n");

            if (save_resume_state() < 0) {

                fprintf(
                    stderr,
                    "FLIPGB_LID suspend save failed\n");

            } else {

                flipgb_exit_code =
                    FLIPGB_EXIT_SWITCHER;

                break;
            }
        }

        if (control_state.system.fps_toggle_requested) {

            fps_overlay_enabled =
                !fps_overlay_enabled;

            flip_video_set_fps_overlay(
                fps_overlay_enabled,
                fps_overlay_value);

            fprintf(
                stderr,
                "FLIPGB: FPS overlay %s\n",
                fps_overlay_enabled
                    ? "ON"
                    : "OFF");
        }

        if (control_state.system.gameswitcher_requested) {

            fprintf(
                stderr,
                "FLIPGB: GameSwitcher requested\n");

            if (save_resume_state() < 0) {

                fprintf(
                    stderr,
                    "FLIPGB_STATE: switcher save failed\n");

            } else {

                flipgb_exit_code =
                    FLIPGB_EXIT_SWITCHER;

                break;
            }
        }

        flip_hotkeys_result_t hotkey_result;

        if (flip_hotkeys_apply_actions(
                &control_state.system,
                &hotkey_result) < 0) {

            fprintf(
                stderr,
                "FLIPGB: global hotkey handling failed\n");
        }

        fps_frames++;'''

    text = regex_replace_once(
        text,
        r'        /\*\n         \* FlipOS lid transition detection\..*?        fps_frames\+\+;',
        actions_block,
        "legacy system-action block",
    )

    exit_block = r'''        /*
         * START+SELECT is interpreted by the shared controls
         * layer as a global FlipOS exit request.
         */
        if (control_state.system.exit_requested) {

            fprintf(
                stderr,
                "FLIPGB: START+SELECT exit\n");

            if (save_resume_state() < 0) {
                fprintf(
                    stderr,
                    "FLIPGB_STATE: exit save failed\n");
            }

            break;
        }

        timespec_add_ns('''

    text = regex_replace_once(
        text,
        r'        /\*\n         \* START \+ SELECT exits the emulator\..*?        timespec_add_ns\(',
        exit_block,
        "legacy START+SELECT block",
    )

    if "FLIP_BTN_" in text:
        locations = [
            line_no
            for line_no, line in enumerate(text.splitlines(), 1)
            if "FLIP_BTN_" in line
        ]
        raise RuntimeError(
            f"physical FLIP_BTN references remain at lines {locations}"
        )

    PATH.write_text(text)
    print("Phase 4A2B Flip_GB migration applied")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as exc:
        print(f"migration refused: {exc}", file=sys.stderr)
        raise SystemExit(1)
