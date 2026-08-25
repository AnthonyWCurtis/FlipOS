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


def replace_span(
    text: str,
    start: str,
    end: str,
    replacement: str,
    label: str,
) -> str:
    start_count = text.count(start)
    end_count = text.count(end)

    if start_count != 1 or end_count != 1:
        raise RuntimeError(
            f"{label}: expected unique markers, "
            f"found start={start_count} end={end_count}"
        )

    start_index = text.index(start)
    end_index = text.index(end, start_index) + len(end)

    return (
        text[:start_index]
        + replacement
        + text[end_index:]
    )


def main() -> int:
    if not PATH.exists():
        print(f"missing {PATH}", file=sys.stderr)
        return 1

    text = PATH.read_text()

    if '#include "flip_timing.h"' in text:
        print("flip_gb.c already contains Phase 4A3 Timing Host migration")
        return 0

    text = replace_once(
        text,
        '#include "flip_controls.h"\n#include "flip_hotkeys.h"',
        '#include "flip_controls.h"\n#include "flip_timing.h"\n#include "flip_hotkeys.h"',
        "timing include",
    )

    text = replace_once(
        text,
        'static flip_controls_state_t control_state;\n',
        'static flip_controls_state_t control_state;\n'
        'static flip_timing_t timing;\n',
        "timing state",
    )

    helper_start = '''static void timespec_add_ns(
    struct timespec *t,
    long long ns)
{'''

    helper_end = '''static int timespec_after(
    const struct timespec *a,
    const struct timespec *b)
{
    if (a->tv_sec != b->tv_sec)
        return a->tv_sec > b->tv_sec;

    return a->tv_nsec > b->tv_nsec;
}'''

    text = replace_span(
        text,
        helper_start,
        helper_end,
        '',
        "local timing helpers",
    )

    init_start = '''    /*
     * Libretro cores do not pace themselves.'''

    init_end = '''    clock_gettime(
        CLOCK_MONOTONIC,
        &fps_started);'''

    init_replacement = '''    /*
     * Libretro cores report their desired frame rate, while
     * the shared FlipOS Timing Host owns the actual monotonic
     * pacing schedule. The GB-specific fallback remains here.
     */
    double fps = av.timing.fps;

    if (fps < 1.0)
        fps = 59.7275;

    if (flip_timing_init(
            &timing,
            fps) < 0) {

        fprintf(
            stderr,
            "FLIPGB: timing initialization failed\\n");

        unload_rom(&game);
        retro_deinit();
        flip_audio_close();
        flip_input_close();
        flip_video_close();

        return 6;
    }

    fprintf(
        stderr,
        "FLIPGB: frame pacing fps=%.6f period_ns=%lld\\n",
        fps,
        timing.frame_period_ns);'''

    text = replace_span(
        text,
        init_start,
        init_end,
        init_replacement,
        "local timing initialization",
    )

    fps_old = '''        fps_frames++;

        struct timespec fps_now;
        clock_gettime(
            CLOCK_MONOTONIC,
            &fps_now);

        double fps_elapsed =
            (double)(fps_now.tv_sec - fps_started.tv_sec) +
            (double)(fps_now.tv_nsec - fps_started.tv_nsec) /
            1000000000.0;

        if (fps_elapsed >= 1.0) {

            measured_fps =
                (double)fps_frames /
                fps_elapsed;

            fps_overlay_value =
                measured_fps;

            flip_video_set_fps_overlay(
                fps_overlay_enabled,
                fps_overlay_value);

            fprintf(
                stderr,
                "FLIPGB_FPS %.3f target=%.3f\\n",
                measured_fps,
                fps);

            fps_frames = 0;
            fps_started = fps_now;
        }'''

    fps_new = '''        double measured_fps = 0.0;

        int fps_result =
            flip_timing_record_frame(
                &timing,
                &measured_fps);

        if (fps_result < 0) {
            fprintf(
                stderr,
                "FLIPGB: timing FPS measurement failed\\n");
            break;
        }

        if (fps_result > 0) {
            fps_overlay_value =
                measured_fps;

            flip_video_set_fps_overlay(
                fps_overlay_enabled,
                fps_overlay_value);

            fprintf(
                stderr,
                "FLIPGB_FPS %.3f target=%.3f\\n",
                measured_fps,
                fps);
        }'''

    text = replace_once(
        text,
        fps_old,
        fps_new,
        "FPS accounting",
    )

    wait_start = '''        timespec_add_ns(
            &next_frame,
            frame_ns);'''

    wait_end = '''        if (timespec_after(
                &now,
                &late_limit)) {

            next_frame = now;

            fprintf(
                stderr,
                "FLIPGB: frame schedule resync\\n");
        }'''

    wait_replacement = '''        int wait_result =
            flip_timing_wait_frame(
                &timing);

        if (wait_result < 0) {
            fprintf(
                stderr,
                "FLIPGB: timing wait failed\\n");
            break;
        }

        if (wait_result > 0) {
            fprintf(
                stderr,
                "FLIPGB: frame schedule resync\\n");
        }'''

    text = replace_span(
        text,
        wait_start,
        wait_end,
        wait_replacement,
        "frame pacing",
    )

    forbidden = (
        "static void timespec_add_ns(",
        "static int timespec_after(",
        "clock_nanosleep(",
        "fps_frames",
        "fps_started",
        "next_frame",
        "frame_ns",
    )

    remaining = [
        token
        for token in forbidden
        if token in text
    ]

    if remaining:
        raise RuntimeError(
            "local timing implementation remains: "
            + ", ".join(remaining)
        )

    PATH.write_text(text)
    print("Phase 4A3 Timing Host migration applied")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as exc:
        print(f"migration refused: {exc}", file=sys.stderr)
        raise SystemExit(1)
