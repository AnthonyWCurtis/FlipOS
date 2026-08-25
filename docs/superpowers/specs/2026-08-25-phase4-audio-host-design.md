# Phase 4A4 — Global Audio Host Design

## Goal

Move emulator audio delivery behind a shared FlipOS Audio Host while preserving the Miyoo Flip RK817/aplay path already proven on real hardware. The milestone must reduce emulator-specific audio ownership and establish a bounded synchronization policy without introducing a new ALSA backend, audio thread, DSP, or speculative resampler.

## Current proven hardware path

Confirmed on the Miyoo Flip:

- PCM device: `hw:0,0`
- Codec: RK817
- PCM format: S16_LE
- Channels: stereo
- Output rate: 32768 Hz
- Deep-resume broken RK817 signature: `17=06 38=00 40=69`
- Recovered signature: `17=40 38=10 40=a5`

The exact RK817 recovery check and writes remain unchanged in this milestone.

## Architecture

```text
Emulator core
    |
    | PCM samples
    v
FlipOS Audio Host
    |- source configuration validation
    |- bounded synchronous staging
    |- per-frame flush boundary
    |- volume/settings ownership
    |- speaker-route setup
    |- RK817 deep-resume recovery
    `- aplay transport -> hw:0,0
```

The Audio Host is the only layer that knows the physical PCM device, output format, output rate, aplay parameters, speaker mixer path, or RK817 recovery sequence.

`Flip_GB` knows only the core-reported sample rate, submits PCM frames, and marks the end of each emulated frame.

## Public API

`include/flip_audio.h` exposes:

```c
typedef struct {
    unsigned sample_rate;
    unsigned channels;
} flip_audio_config_t;

int flip_audio_open(
    const flip_audio_config_t *config);

size_t flip_audio_submit(
    const int16_t *samples,
    size_t frames);

int flip_audio_end_frame(void);

void flip_audio_close(void);

int flip_audio_set_volume(int percent);
int flip_audio_get_volume(void);
```

The old `flip_audio_write()` API is removed after all current emulator callers are migrated.

## Configuration policy

For this milestone the host accepts only the hardware-proven native configuration:

- sample rate: 32768 Hz
- channels: 2
- sample format: signed 16-bit little-endian, implicit in the API

Unsupported configurations fail `flip_audio_open()` cleanly. The emulator continues without audio, matching the existing silent-fallback policy.

No resampler is introduced in 4A4. A future resampler belongs inside the Audio Host when a second real core proves it is required.

## Buffering and synchronization policy

Audio remains synchronous with the emulation thread. There is no worker thread and no long-lived asynchronous queue.

The host uses a fixed 256-frame stereo staging buffer. At 32768 Hz this represents at most 7.8125 ms of staged audio.

`flip_audio_submit()` appends PCM frames to this buffer. When the buffer fills it is written synchronously to the proven aplay pipe. Large submissions are therefore drained in bounded chunks rather than accumulating latency.

`flip_audio_end_frame()` flushes any remaining staged samples after each `retro_run()` call. This establishes the synchronization policy:

- FlipOS Timing Host remains the video/emulation clock master.
- Core-produced audio is never allowed to accumulate across emulated frame boundaries in FlipOS-owned memory.
- No hidden multi-frame software queue is introduced.
- Existing aplay/ALSA hardware buffering remains unchanged for this milestone.

This is deliberately a bounded baseline, not drift correction. Audio-rate conversion or dynamic synchronization is deferred until measurements show it is required.

## Internal buffer component

A hardware-independent helper lives in:

- `src/audio/flip_audio_buffer.h`
- `src/audio/flip_audio_buffer.c`

It owns the 256-frame staging array, submission accounting, bounded chunk flushing, and end-frame drain behavior. A sink callback makes this logic native-host-testable without RK817 or ALSA hardware.

The helper is internal to the audio implementation and is not a public FlipOS runtime API.

## Transport behavior

The existing aplay transport is retained:

```text
/usr/bin/aplay -q -D hw:0,0 -t raw -f S16_LE -c 2 -r 32768 -B 50000 -F 10000
```

The host continues to ignore SIGPIPE, handles EINTR, detects EPIPE, and closes the dead transport rather than terminating the emulator.

No direct `libasound` dependency is added in 4A4.

## RK817 recovery

The existing recovery logic remains evidence-gated. It reads only registers `0x17`, `0x38`, and `0x40` and writes recovery values only when all three exactly match the confirmed broken post-deep-resume signature.

No speculative codec programming is permitted.

## Flip_GB integration

`Flip_GB` changes are intentionally small:

1. Obtain `retro_system_av_info` before opening audio.
2. Round the reported audio sample rate to an integer configuration value.
3. Open the Audio Host with that rate and two channels.
4. Route both libretro audio callbacks to `flip_audio_submit()`.
5. Immediately after each `retro_run()`, call `flip_audio_end_frame()`.
6. Keep silent fallback if audio initialization fails.

Audio flush occurs before handling GameSwitcher, lid, or exit actions generated by that frame, so the final frame cannot leave PCM stranded in the staging buffer.

## Error handling

- Invalid configuration: open fails before touching hardware.
- Speaker setup failure: logged, then transport opening continues as today.
- Pipe/fork failure: open fails.
- RK817 recovery check failure: logged; playback continues as today.
- Transport write failure: the transport is marked unavailable and buffered PCM is discarded; emulation continues silently.
- Closing audio flushes any remaining staged PCM when the transport is still available, then closes/waits for aplay.

## Tests

Native host tests cover the hardware-independent buffer component:

- submit smaller than capacity stages without early sink writes
- end-frame flush delivers exactly the staged frames
- submissions larger than capacity flush in bounded chunks without dropping/reordering samples
- sink failure is reported
- reset clears staged frames

Build/device gates:

- `make test-controls`
- `make test-timing`
- `make test-audio`
- `make all`
- `git diff --check`
- physical Miyoo Flip acceptance

## Hardware acceptance

On the real Miyoo Flip:

1. GB audio starts normally.
2. Audio remains approximately synchronized during extended play.
3. No new crackles/dropouts are introduced.
4. FPS remains approximately 59.7.
5. Volume controls work.
6. MENU/GameSwitcher/resume restores audio.
7. Lid close/deep suspend/open restores audio through the proven RK817 recovery path.
8. Controls and START+SELECT behavior remain unchanged.

## Out of scope

- direct libasound backend
- asynchronous audio worker/thread
- large ring buffer
- sample-rate conversion/resampling
- time stretching
- dynamic A/V drift correction
- lifecycle ownership changes
- performance/governor tuning
