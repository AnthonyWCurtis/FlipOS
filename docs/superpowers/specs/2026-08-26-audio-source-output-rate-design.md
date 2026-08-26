# FlipOS Audio Source / Output Rate Design

## Status

Hardware verified on the Miyoo Flip on 2026-08-26.

## Confirmed Problem

Gambatte produces native stereo S16 PCM at 32768 Hz. FlipOS previously passed that rate directly to `aplay -D hw:0,0`.

Hardware probing on the Miyoo Flip showed:

- a 32768 Hz playback request is negotiated by the RK817/ASoC path to 32000 Hz;
- a direct 32000 Hz request runs at exactly 32000 Hz;
- during the pre-fix run the ALSA buffer became nearly full and synchronous pipe writes rose from microseconds to roughly 10–19 ms;
- the same behavior reproduced on Phase 4A5 and Phase 4B, so it is not a Core Adapter API regression.

The clock-domain mismatch is therefore:

```text
core source:      32768 frames/sec
physical output:  32000 frames/sec
excess source:      768 frames/sec
```

This creates progressive audio backpressure until the synchronous transport blocks inside the core callback path.

## Architectural Rule

The emulator owns its native source rate. FlipOS owns conversion to the handheld's physical playback rate.

A core must not be retimed merely to match the Miyoo Flip codec.

For Gambatte:

```text
Gambatte 32768 Hz PCM
        -> FlipOS streaming resampler
        -> existing bounded Audio Host staging
        -> aplay hw:0,0 at 32000 Hz
        -> RK817
```

## Configuration

`flip_audio_config_t` distinguishes:

- `source_sample_rate`
- `output_sample_rate`
- `channels`

Gambatte's source rate continues to come from libretro timing metadata. The current Miyoo Flip hardware output policy is 32000 Hz stereo.

## Resampler

Use a small streaming linear interpolator with persistent rational phase.

Requirements:

- stereo signed 16-bit PCM;
- integer/rational phase using 64-bit arithmetic;
- state preserved across libretro callbacks and emulator frames;
- no per-frame heap allocation;
- no worker thread;
- no large queue;
- bounded caller-provided output storage;
- exact long-term source/output frame ratio;
- resumable processing when output capacity is exhausted;
- equal-rate operation remains valid;
- no external resampling dependency.

For Gambatte the exact ratio is:

```text
32000 / 32768 = 125 / 128
```

## Transport

The physical transport requests exactly:

```text
hw:0,0
S16_LE
stereo
32000 Hz
-B 50000
-F 10000
```

Buffer and period tuning is outside this fix. The rate mismatch must be removed first.

## RK817 Recovery

The existing deep-resume recovery policy is unchanged. Recovery remains restricted to the confirmed broken register signature:

```text
0x17 = 0x06
0x38 = 0x00
0x40 = 0x69
```

No broader codec writes are introduced.

## Verification

Software verification covers:

- equal-rate identity;
- exact 32768 -> 32000 long-term frame count;
- chunk-boundary equivalence;
- stereo preservation;
- reset behavior;
- bounded output-capacity resume;
- Audio Host source/output configuration separation;
- physical 32000 Hz transport policy.

### Hardware verification — 2026-08-26

Verified on the real Miyoo Flip using Pokémon Yellow with the Phase 4B diagnostic instrumentation enabled.

Observed live PCM state:

```text
aplay -q -D hw:0,0 -t raw -f S16_LE -c 2 -r 32000 -B 50000 -F 10000
rate: 32000 (32000/1)
period_size: 320
buffer_size: 1600
state: RUNNING
```

Long-run current-session diagnostics after the previous approximately 16-second backlog onset showed:

- measured FPS averaged approximately 59.727, matching the 59.7275 target;
- zero frame-schedule resyncs across the captured current-session windows;
- synchronous transport `write_max_us` remained in the tens to hundreds of microseconds, with the captured worst value below 0.5 ms;
- the previous 10–19 ms transport stalls did not recur;
- ALSA buffer occupancy moved in both directions during repeated one-second sampling rather than progressively filling;
- audio pitch, speed and synchronization were subjectively confirmed correct with no reported crackling or progressive drift.

Deep-suspend/resume was also verified. After lid close/open the exact confirmed broken RK817 signature was observed:

```text
0x17 = 0x06
0x38 = 0x00
0x40 = 0x69
```

The existing restricted recovery path applied successfully, playback reopened as:

```text
source_rate=32768 output_rate=32000
```

and the saved session resumed with correct audio and low transport-write latency.

Result: the 32768 Hz source / 32000 Hz hardware clock-domain mismatch is considered hardware resolved.

## Safety and Scope

All work remains SD-card-only. Internal NAND/MTD is not modified. No new emulator core, async audio worker, large queue, timing redesign, or save-state change is part of this fix.
