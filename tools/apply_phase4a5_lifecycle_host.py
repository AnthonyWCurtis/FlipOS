#!/usr/bin/env python3

from pathlib import Path
import sys

PATH = Path("src/emulators/flip_gb.c")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)

    if count != 1:
        raise RuntimeError(
            f"{label}: expected exactly one match, found {count}"
        )

    return text.replace(old, new, 1)


def main() -> int:
    if not PATH.exists():
        print(f"missing {PATH}", file=sys.stderr)
        return 1

    text = PATH.read_text()

    if (
        '#include "flip_lifecycle.h"' in text
        and text.count("flip_lifecycle_handle(") == 2
        and "FLIPGB_EXIT_SWITCHER" not in text
    ):
        print("flip_gb.c already contains Phase 4A5 Lifecycle Host migration")
        return 0

    text = replace_once(
        text,
        '#include "flip_timing.h"\n#include "flip_hotkeys.h"',
        '#include "flip_timing.h"\n#include "flip_lifecycle.h"\n#include "flip_hotkeys.h"',
        "Lifecycle Host include",
    )

    text = replace_once(
        text,
        '\n#define FLIPGB_EXIT_SWITCHER 20\n',
        '',
        "local GameSwitcher exit code",
    )

    old_save_tail = '''    fprintf(
        stderr,
        "FLIPGB_STATE: SAVE_OK path=%s size=%zu\\n",
        path,
        size);

    return 0;
}

static int load_resume_state(void)
'''

    new_save_tail = '''    fprintf(
        stderr,
        "FLIPGB_STATE: SAVE_OK path=%s size=%zu\\n",
        path,
        size);

    return 0;
}

static int lifecycle_save_cb(
    void *userdata)
{
    (void)userdata;

    return save_resume_state();
}

static int load_resume_state(void)
'''

    text = replace_once(
        text,
        old_save_tail,
        new_save_tail,
        "Lifecycle Host save callback",
    )

    old_timing_failure = '''        unload_rom(&game);
        retro_deinit();
        flip_audio_close();
        flip_input_close();
        flip_video_close();

        return 6;
'''

    new_timing_failure = '''        unload_rom(&game);
        retro_deinit();

        if (flip_lifecycle_shutdown_shared() < 0) {
            fprintf(
                stderr,
                "FLIPGB: shared lifecycle cleanup warning\\n");
        }

        return 6;
'''

    text = replace_once(
        text,
        old_timing_failure,
        new_timing_failure,
        "timing failure cleanup",
    )

    old_frame_pacing = '''    fprintf(
        stderr,
        "FLIPGB: frame pacing fps=%.6f period_ns=%lld\\n",
        fps,
        timing.frame_period_ns);

    while (1) {
'''

    new_frame_pacing = '''    fprintf(
        stderr,
        "FLIPGB: frame pacing fps=%.6f period_ns=%lld\\n",
        fps,
        timing.frame_period_ns);

    if (resume_result > 0) {

        if (flip_lifecycle_resume_timing(
                &timing) < 0) {

            fprintf(
                stderr,
                "FLIPGB: lifecycle timing resync failed\\n");
        }
    }

    while (1) {
'''

    text = replace_once(
        text,
        old_frame_pacing,
        new_frame_pacing,
        "resume timing resync",
    )

    old_immediate = '''        if (control_state.system.lid_changed) {

            fprintf(
                stderr,
                "FLIPGB_LID %s\\n",
                control_state.system.lid_closed
                    ? "CLOSED"
                    : "OPEN");
        }

        if (control_state.system.suspend_requested) {

            fprintf(
                stderr,
                "FLIPGB_LID suspend requested\\n");

            if (save_resume_state() < 0) {

                fprintf(
                    stderr,
                    "FLIPGB_LID suspend save failed\\n");

            } else {

                flipgb_exit_code =
                    FLIPGB_EXIT_SWITCHER;

                break;
            }
        }
'''

    new_immediate = '''        flip_lifecycle_result_t
            lifecycle_immediate;

        int lifecycle_result =
            flip_lifecycle_handle(
                &control_state.system,
                FLIP_LIFECYCLE_CHECKPOINT_IMMEDIATE,
                lifecycle_save_cb,
                NULL,
                &lifecycle_immediate);

        if (lifecycle_result < 0) {
            fprintf(
                stderr,
                "FLIPGB: lifecycle immediate handling failed\\n");
            break;
        }

        if (lifecycle_immediate.should_exit) {
            flipgb_exit_code =
                lifecycle_immediate.exit_code;
            break;
        }
'''

    text = replace_once(
        text,
        old_immediate,
        new_immediate,
        "immediate lifecycle policy",
    )

    old_switcher = '''        if (control_state.system.gameswitcher_requested) {

            fprintf(
                stderr,
                "FLIPGB: GameSwitcher requested\\n");

            if (save_resume_state() < 0) {

                fprintf(
                    stderr,
                    "FLIPGB_STATE: switcher save failed\\n");

            } else {

                flipgb_exit_code =
                    FLIPGB_EXIT_SWITCHER;

                break;
            }
        }

'''

    text = replace_once(
        text,
        old_switcher,
        '',
        "local GameSwitcher policy",
    )

    old_exit = '''        /*
         * START+SELECT is interpreted by the shared controls
         * layer as a global FlipOS exit request.
         */
        if (control_state.system.exit_requested) {

            fprintf(
                stderr,
                "FLIPGB: START+SELECT exit\\n");

            if (save_resume_state() < 0) {
                fprintf(
                    stderr,
                    "FLIPGB_STATE: exit save failed\\n");
            }

            break;
        }

'''

    new_exit = '''        flip_lifecycle_result_t
            lifecycle_after_frame;

        lifecycle_result =
            flip_lifecycle_handle(
                &control_state.system,
                FLIP_LIFECYCLE_CHECKPOINT_AFTER_FRAME,
                lifecycle_save_cb,
                NULL,
                &lifecycle_after_frame);

        if (lifecycle_result < 0) {
            fprintf(
                stderr,
                "FLIPGB: lifecycle after-frame handling failed\\n");
            break;
        }

        if (lifecycle_after_frame.should_exit) {
            flipgb_exit_code =
                lifecycle_after_frame.exit_code;
            break;
        }

'''

    text = replace_once(
        text,
        old_exit,
        new_exit,
        "after-frame lifecycle policy",
    )

    old_final_cleanup = '''    unload_rom(&game);

    retro_deinit();

    flip_audio_close();

    flip_input_close();
    flip_video_close();

    fprintf(
'''

    new_final_cleanup = '''    unload_rom(&game);

    retro_deinit();

    if (flip_lifecycle_shutdown_shared() < 0) {
        fprintf(
            stderr,
            "FLIPGB: shared lifecycle cleanup warning\\n");
    }

    fprintf(
'''

    text = replace_once(
        text,
        old_final_cleanup,
        new_final_cleanup,
        "normal shared cleanup",
    )

    forbidden = [
        "FLIPGB_EXIT_SWITCHER",
        "FLIPGB_LID suspend requested",
        "FLIPGB_STATE: switcher save failed",
        "FLIPGB: START+SELECT exit",
    ]

    for token in forbidden:
        if token in text:
            raise RuntimeError(
                f"legacy lifecycle token remains: {token}"
            )

    if text.count("flip_lifecycle_handle(") != 2:
        raise RuntimeError(
            "expected exactly two flip_lifecycle_handle calls"
        )

    if text.count("flip_lifecycle_shutdown_shared(") != 2:
        raise RuntimeError(
            "expected exactly two shared cleanup calls"
        )

    PATH.write_text(text)

    print("Phase 4A5 Lifecycle Host migration applied")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as exc:
        print(f"migration refused: {exc}", file=sys.stderr)
        raise SystemExit(1)
