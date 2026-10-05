from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = (ROOT / "include" / "flip_audio.h").read_text()
AUDIO = (ROOT / "src" / "audio" / "flip_audio.c").read_text()
CORE_HOST = (ROOT / "src" / "runtime" / "flip_core_host.c").read_text()
LIBRETRO = (ROOT / "src" / "emulators" / "flip_libretro_core.c").read_text()

assert "source_sample_rate" in HEADER, "Audio config must expose core/native source rate"
assert "output_sample_rate" in HEADER, "Audio config must expose physical output rate"
assert "unsigned sample_rate;" not in HEADER, "single audio sample_rate conflates clock domains"

assert "config->output_sample_rate != 32000" in AUDIO, (
    "Audio Host must constrain Miyoo Flip physical output rate to verified 32000 Hz"
)
assert '"-r"' in AUDIO and "output_rate_text" in AUDIO, (
    "aplay transport must receive the configured physical output rate"
)
assert '"32768"' not in AUDIO, "Audio Host must not request 32768 Hz from RK817 hardware"

assert ".source_sample_rate = info.audio_sample_rate" in CORE_HOST, (
    "Core Host must preserve libretro/native source sample rate"
)
assert ".output_sample_rate = 32000" in CORE_HOST, (
    "Core Host must apply Miyoo Flip physical output-rate policy"
)

assert "flip_audio_set_source_rate" in HEADER, (
    "Audio Host must expose runtime source-rate retuning"
)
assert "RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO" in LIBRETRO, (
    "Libretro bridge must handle runtime AV timing changes"
)
assert "flip_audio_set_source_rate(" in LIBRETRO, (
    "Libretro runtime AV changes must retune the shared Audio Host"
)
assert "runtime_audio_sample_rate" in LIBRETRO, (
    "Libretro bridge must remember the latest runtime audio rate"
)

print("audio rate conversion architecture tests passed")
