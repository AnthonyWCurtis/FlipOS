from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = (ROOT / "include" / "flip_audio.h").read_text()
AUDIO = (ROOT / "src" / "audio" / "flip_audio.c").read_text()
CORE_HOST = (ROOT / "src" / "runtime" / "flip_core_host.c").read_text()

assert "source_sample_rate" in HEADER, "Audio config must expose core/native source rate"
assert "output_sample_rate" in HEADER, "Audio config must expose physical output rate"
assert "unsigned sample_rate;" not in HEADER, "single audio sample_rate conflates clock domains"

assert '"32000"' in AUDIO, "physical aplay transport must request verified 32000 Hz"
assert '"32768"' not in AUDIO, "Audio Host must not request 32768 Hz from RK817 hardware"

assert ".source_sample_rate = info.audio_sample_rate" in CORE_HOST, (
    "Core Host must preserve libretro/native source sample rate"
)
assert ".output_sample_rate = 32000" in CORE_HOST, (
    "Core Host must apply Miyoo Flip physical output-rate policy"
)

print("audio rate conversion architecture tests passed")
