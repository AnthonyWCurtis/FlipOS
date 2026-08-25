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
        "flip_audio_submit(" in text
        and "flip_audio_end_frame(" in text
        and "flip_audio_write(" not in text
    ):
        print("flip_gb.c already contains Phase 4A4 Audio Host migration")
        return 0

    old_callbacks = '''static void audio_sample_cb(
    int16_t left,
    int16_t right)
{
    int16_t frame[2];

    frame[0] = left;
    frame[1] = right;

    flip_audio_write(
        frame,
        1);
}

static size_t audio_batch_cb(
    const int16_t *data,
    size_t frames)
{
    return flip_audio_write(
        data,
        frames);
}
'''

    new_callbacks = '''static void audio_sample_cb(
    int16_t left,
    int16_t right)
{
    int16_t frame[2];

    frame[0] = left;
    frame[1] = right;

    flip_audio_submit(
        frame,
        1);
}

static size_t audio_batch_cb(
    const int16_t *data,
    size_t frames)
{
    return flip_audio_submit(
        data,
        frames);
}
'''

    text = replace_once(
        text,
        old_callbacks,
        new_callbacks,
        "audio callbacks",
    )

    old_early_open = '''    if (flip_audio_open() < 0) {
        fprintf(
            stderr,
            "FLIPGB: audio initialization failed; "
            "continuing silent\\n");
    }

'''

    text = replace_once(
        text,
        old_early_open,
        "",
        "early audio open",
    )

    old_av_boundary = '''    fprintf(
        stderr,
        "FLIPGB: video base=%ux%u max=%ux%u fps=%.3f audio=%.1f\\n",
        av.geometry.base_width,
        av.geometry.base_height,
        av.geometry.max_width,
        av.geometry.max_height,
        av.timing.fps,
        av.timing.sample_rate);

    /*
     * Libretro cores report their desired frame rate, while
'''

    new_av_boundary = '''    fprintf(
        stderr,
        "FLIPGB: video base=%ux%u max=%ux%u fps=%.3f audio=%.1f\\n",
        av.geometry.base_width,
        av.geometry.base_height,
        av.geometry.max_width,
        av.geometry.max_height,
        av.timing.fps,
        av.timing.sample_rate);

    /*
     * The core reports its source audio rate. Physical PCM
     * configuration and RK817 transport belong to FlipOS.
     */
    flip_audio_config_t audio_config = {
        .sample_rate =
            (unsigned)(
                av.timing.sample_rate + 0.5),
        .channels = 2,
    };

    if (flip_audio_open(
            &audio_config) < 0) {

        fprintf(
            stderr,
            "FLIPGB: audio initialization failed; "
            "continuing silent\\n");
    }

    /*
     * Libretro cores report their desired frame rate, while
'''

    text = replace_once(
        text,
        old_av_boundary,
        new_av_boundary,
        "audio configuration boundary",
    )

    old_run_boundary = '''        retro_run();

        /*
         * FlipOS logical system actions.
'''

    new_run_boundary = '''        retro_run();

        /*
         * Flush all PCM produced by this emulated frame before
         * handling lifecycle/system actions for that frame.
         */
        if (flip_audio_end_frame() < 0) {

            fprintf(
                stderr,
                "FLIPGB: audio frame flush failed\\n");
        }

        /*
         * FlipOS logical system actions.
'''

    text = replace_once(
        text,
        old_run_boundary,
        new_run_boundary,
        "per-frame audio flush",
    )

    if "flip_audio_write(" in text:
        raise RuntimeError("legacy flip_audio_write call remains")

    if "flip_audio_open()" in text:
        raise RuntimeError("legacy no-argument flip_audio_open remains")

    if text.count("flip_audio_end_frame(") != 1:
        raise RuntimeError(
            "expected exactly one flip_audio_end_frame call"
        )

    PATH.write_text(text)
    print("Phase 4A4 Audio Host migration applied")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as exc:
        print(f"migration refused: {exc}", file=sys.stderr)
        raise SystemExit(1)
