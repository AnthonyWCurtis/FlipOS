from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

AUDIO_H = (ROOT / "include" / "flip_audio.h").read_text()
AUDIO_C = (ROOT / "src" / "audio" / "flip_audio.c").read_text()
CORE_HOST_C = (ROOT / "src" / "runtime" / "flip_core_host.c").read_text()


def require(text, needle, label):
    assert needle in text, f"missing {label}: {needle}"


def main():
    require(AUDIO_H, "flip_audio_diag_snapshot_t", "audio diagnostic snapshot type")
    require(AUDIO_H, "flip_audio_diag_snapshot", "audio diagnostic snapshot API")
    require(AUDIO_H, "flip_audio_diag_reset", "audio diagnostic reset API")

    require(AUDIO_C, "submit_total_us", "audio submit total timing accumulator")
    require(AUDIO_C, "submit_max_us", "audio submit maximum timing accumulator")
    require(AUDIO_C, "transport_write_total_us", "transport write total timing accumulator")
    require(AUDIO_C, "transport_write_max_us", "transport write maximum timing accumulator")
    require(AUDIO_C, "transport_write_count", "transport write counter")

    require(CORE_HOST_C, "FLIP_AUDIO_PERF:", "audio callback performance summary")
    require(CORE_HOST_C, "submit_avg_us=", "audio submit average timing summary")
    require(CORE_HOST_C, "submit_max_us=", "audio submit maximum timing summary")
    require(CORE_HOST_C, "write_avg_us=", "transport write average timing summary")
    require(CORE_HOST_C, "write_max_us=", "transport write maximum timing summary")
    require(CORE_HOST_C, "writes=", "transport write count summary")

    print("audio callback diagnostics architecture tests passed")


if __name__ == "__main__":
    main()
