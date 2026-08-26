# FlipOS Audio Source / Output Rate Design

## Status

Approved architectural design for resolving the confirmed Miyoo Flip
audio-rate mismatch.

## Problem

FlipOS currently treats the emulator core audio sample rate as though it
were also the physical PCM playback rate.

Gambatte reports a native source rate of:

    32768 Hz

FlipOS therefore launches:

    aplay ... -r 32768

Hardware measurements on the Miyoo Flip show that RK817 playback does not
actually run at 32768 Hz. ALSA negotiates that request to:

    32000 Hz

A direct 32000 Hz request also runs at exactly 32000 Hz.

This creates a producer/consumer mismatch:

    source:   32768 frames/sec
    hardware: 32000 frames/sec

The source therefore produces 768 stereo frames/sec more than the physical
PCM device consumes.

Observed hardware behaviour confirms the consequence:

- ALSA playback buffer becomes nearly full.
- synchronous pipe writes begin blocking for roughly 10-19 ms.
- the blocking occurs inside retro_run() through flip_audio_submit().
- emulator frame execution becomes late.
- FlipOS timing resyncs increase and measured FPS falls.

The same behaviour was reproduced on both Phase 4A5 and Phase 4B. Therefore
this is not a Core Adapter API regression; it is an existing Audio Host
rate-boundary problem exposed by the new diagnostics.

## Architectural Rule

Emulator source audio rate and physical hardware playback rate are separate
concepts.

The emulator core owns its native source rate.

FlipOS owns conversion from that native source rate to the Miyoo Flip
physical PCM rate.

A core must never be modified merely to match the handheld audio codec.

## Data Flow

    Emulator core
        |
        | native stereo S16 PCM
        | source rate supplied by core
        v
    FlipOS Audio Host
        |
        | streaming source-rate -> output-rate conversion
        v
    existing small staging / bounded synchronous transport
        |
        | stereo S16 PCM at physical rate
        v
    aplay -D hw:0,0 -r 32000
        |
        v
    RK817 PCM

For Gambatte:

    32768 Hz -> 32000 Hz

## Audio Configuration

The Audio Host configuration must explicitly distinguish:

    source_sample_rate
    output_sample_rate
    channels

For the current Miyoo Flip hardware:

    output_sample_rate = 32000
    channels = 2

The core adapter supplies the source sample rate from libretro metadata.

The physical output rate remains FlipOS hardware policy and must not be
encoded into Gambatte.

## Resampler

The initial implementation will use a small streaming linear interpolator.

Requirements:

- stereo signed 16-bit PCM
- persistent phase across submit calls
- no heap allocation in the per-frame path
- no worker thread
- no large audio queue
- bounded temporary storage
- deterministic output count
- continuous phase across libretro callbacks
- reset phase on audio open/close
- support equal-rate pass-through
- support arbitrary positive source/output integer rates rather than
  hard-coding Gambatte's 32768 -> 32000 ratio

For Gambatte the exact ratio is:

    32000 / 32768 = 125 / 128

The converter must therefore produce exactly 125 output frames for every
128 source-frame intervals over steady-state operation, without accumulating
long-term rate error.

Linear interpolation is sufficient for the first implementation because the
rate difference is small and the goal is correct clock-domain conversion
with minimal CPU and memory cost on RK3566.

No external resampling dependency will be introduced.

## Transport

The physical transport must request exactly:

    32000 Hz
    S16_LE
    stereo

The existing Audio Host transport remains synchronous and bounded.

The current aplay buffering policy remains unchanged initially:

    -B 50000
    -F 10000

Changing transport buffering is explicitly outside the first fix. The
rate mismatch must be removed before considering any buffer tuning.

## Existing Audio Recovery

RK817 suspend/resume recovery behaviour must remain unchanged.

The known exact broken register signature remains:

    0x17 = 0x06
    0x38 = 0x00
    0x40 = 0x69

Recovery writes remain restricted to that exact signature.

The resampler must not broaden or alter codec recovery logic.

## Diagnostics

Existing FLIP_AUDIO_PERF diagnostics remain available during verification.

They continue measuring synchronous submit and transport write behaviour.

The implementation may additionally expose source/output frame totals for
tests, but permanent runtime logging beyond the existing diagnostics is not
required.

## Testing

Development follows TDD.

Unit tests must verify:

1. equal-rate input produces identical output.
2. 32768 -> 32000 maintains the correct long-term frame ratio.
3. conversion state survives arbitrary submit chunk boundaries.
4. stereo channels remain paired correctly.
5. interpolation remains within signed 16-bit range.
6. resetting the converter clears fractional phase/history.
7. no cumulative frame-count drift occurs over a long simulated stream.

Audio Host architecture tests must verify:

- source and output rates are represented separately.
- physical aplay transport requests 32000 Hz.
- core/native rate is not overwritten with 32000 Hz.

Existing audio buffer, lifecycle, timing and architecture tests must remain
green.

## Hardware Verification

Hardware acceptance requires a run substantially longer than the previously
observed approximately 16-second buffer-fill period.

Verify:

    /proc/asound/card0/pcm0p/sub0/hw_params
        rate: 32000

and confirm:

- no continuously accumulating PCM backlog
- audio remains correct in pitch and speed
- no audible discontinuities
- no progressive A/V drift
- synchronous write spikes are materially reduced
- sustained FPS returns close to the 59.7275 Hz target
- timing resync rate drops materially
- suspend/resume audio recovery still works
- GameSwitcher/lifecycle behaviour remains unchanged

## Scope

Included:

- source/output rate separation
- small shared streaming resampler
- 32000 Hz Miyoo Flip physical playback policy
- Gambatte hardware verification

Excluded:

- async audio worker
- large queues
- ALSA plug-based resampling
- new emulator cores
- audio latency tuning
- unrelated timing changes
- save/state changes
- NAND/MTD changes

## Safety

All development remains SD-card-only.

Internal NAND/MTD is not modified.

The stock Miyoo environment remains the known-good recovery path.
