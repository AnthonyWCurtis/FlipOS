# Audio Source / Output Rate Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Convert emulator-native PCM to the Miyoo Flip physical 32000 Hz playback rate without changing emulator timing or introducing audio backlog.

**Architecture:** Cores retain their native source rate. The shared Audio Host performs streaming source-rate to output-rate conversion before the existing bounded synchronous transport.

**Tech Stack:** C11, libretro audio callbacks, FlipOS Audio Host, `aplay`, ALSA `hw:0,0`, RK817.

**Spec:** `docs/superpowers/specs/2026-08-26-audio-source-output-rate-design.md`

## Global Constraints

- SD-card-only development.
- No NAND/MTD changes.
- Gambatte source remains 32768 Hz.
- Miyoo Flip physical output is 32000 Hz.
- RK817 exact-signature recovery remains unchanged.
- Keep `-B 50000 -F 10000` initially.
- No worker thread, large queue, or external resampler dependency.
- Do not modify timing, video, lifecycle, or save-state behavior as part of this fix.

## Task 1 — Streaming Resampler

Files:

- `include/flip_audio_resampler.h`
- `src/audio/flip_audio_resampler.c`
- `tests/test_flip_audio_resampler.c`
- `Makefile`

TDD requirements:

- [x] RED test exists before production converter.
- [x] Equal-rate identity.
- [x] 32768 input frames produce exactly 32000 output frames.
- [x] Arbitrary callback chunk boundaries preserve output.
- [x] Output-capacity exhaustion resumes without dropping input.
- [x] Reset clears stream phase/history.
- [x] Stereo S16 path uses 64-bit interpolation arithmetic.

## Task 2 — Separate Clock Domains

Files:

- `include/flip_audio.h`
- `src/runtime/flip_core_host.c`
- `tests/test_audio_rate_conversion_architecture.py`

Requirements:

- [x] Replace the single `sample_rate` field with `source_sample_rate` and `output_sample_rate`.
- [x] Preserve `info.audio_sample_rate` as the core/native source rate.
- [x] Set current Miyoo Flip output policy to 32000 Hz.

## Task 3 — Audio Host Integration

File:

- `src/audio/flip_audio.c`

Requirements:

- [x] Initialize persistent resampler state in `flip_audio_open()`.
- [x] Convert PCM before `flip_audio_buffer_submit()`.
- [x] Preserve `flip_audio_submit()` return units as source frames.
- [x] Do not reset phase at emulator frame boundaries.
- [x] Reset converter on open/close or invalidated transport.
- [x] Keep RK817 recovery policy unchanged.

## Task 4 — Exact Physical Transport Rate

Requirements:

- [x] `aplay` uses the configured output rate.
- [x] Current output is 32000 Hz.
- [x] `-B 50000 -F 10000` remains unchanged.
- [x] Stale 32768 Hz physical-hardware comment is corrected.

## Task 5 — Software Verification

Run on the WSL checkout after fetching the feature branch:

```bash
make clean
make test-audio-resampler
make test-audio
make test-architecture
make all
git diff --check
```

Expected: all tests pass; the only accepted build warning is the already-known static glibc `gethostbyname` warning.

## Task 6 — Hardware Verification

Deploy `build/flip-gb` atomically to the SD card and confirm all SHA-256 hashes match.

While Pokémon Yellow runs, verify:

```bash
cat /proc/asound/card0/pcm0p/sub0/hw_params
cat /proc/asound/card0/pcm0p/sub0/status
```

Required physical rate:

```text
rate: 32000
```

Run beyond the previous approximately 16-second backlog onset and collect:

- `FLIP_AUDIO_PERF`
- `FLIP_CORE_HOST_FPS`
- `FLIP_PERF`
- frame schedule resyncs

Acceptance requires materially reduced write stalls/resyncs, no progressive backlog or A/V drift, correct pitch/speed, and unchanged suspend/resume and GameSwitcher behavior.

## Task 7 — Promotion Decision

Do not merge automatically. Promote only after real-device hardware verification passes. If backlog persists after exact rate conversion, return to systematic debugging rather than adding speculative asynchronous buffering or timing changes.
