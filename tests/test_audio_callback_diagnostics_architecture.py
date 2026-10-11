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

    require(AUDIO_C, "static int mixer_fd = -1", "cached ALSA mixer fd")
    require(AUDIO_C, "alsa_control_close();", "mixer fd cleanup")
    require(AUDIO_C, "Reassert the RK817 speaker route at audio-open time", "direct startup mixer setup")
    assert "system(command)" not in AUDIO_C, "audio mixer path must not spawn amixer"

    print("audio callback diagnostics architecture tests passed")


    require(AUDIO_C, 'ALSA_CONTROL_DEVICE "/dev/snd/controlC0"', "direct ALSA control device")
    require(AUDIO_C, "SNDRV_CTL_IOCTL_ELEM_WRITE", "direct ALSA control writes")
    require(AUDIO_C, "ALSA_NUMID_SPK_VOLUME", "RK817 speaker volume control")
    require(AUDIO_C, "percent > 0 && old_volume > 0", "fast nonzero volume path")
    require(AUDIO_C, "direct volume control failed", "direct volume failure diagnostics")


if __name__ == "__main__":
    main()
