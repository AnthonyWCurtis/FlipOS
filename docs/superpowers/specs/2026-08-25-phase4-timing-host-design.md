# Phase 4A3 Global Timing Host Design

## Goal

Move frame pacing, monotonic scheduling, late-frame resynchronisation and measured-FPS accounting out of `Flip_GB` into a reusable FlipOS runtime timing host without changing the proven Game Boy timing behaviour.

## Scope

Phase 4A3 is an extraction/refactor milestone. It does not redesign audio, add frame skipping, add adaptive pacing, change emulation speed, introduce display-vsync control, or move lifecycle policy.

The current Game Boy target rate remains sourced from Gambatte through `retro_get_system_av_info()`. If the core reports an invalid FPS, `Flip_GB` continues to select the Game Boy-specific fallback of `59.7275` FPS before creating the timing host. The shared host must never contain a Game Boy-specific fallback.

## Ownership

### `Flip_GB`

- obtains the core-reported target FPS
- applies the Game Boy fallback if required
- calls the shared timing host once per completed emulated frame
- displays/logs measured FPS supplied by the host
- decides whether the emulator is exiting before waiting for another frame

### `flip_timing`

- converts target FPS to a nanosecond frame period
- owns the absolute `CLOCK_MONOTONIC` deadline
- owns frame-count/FPS measurement state
- performs absolute sleeps with `clock_nanosleep(..., TIMER_ABSTIME, ...)`
- detects when execution is more than approximately one frame late
- resynchronises the absolute schedule rather than allowing catch-up bursts
- exposes an explicit resync operation for later lifecycle integration

## Public API

`include/flip_timing.h` defines:

```c
typedef struct {
    double target_fps;
    long long frame_period_ns;
    struct timespec next_frame;
    struct timespec fps_started;
    unsigned long long fps_frames;
    double measured_fps;
} flip_timing_t;

int flip_timing_init(flip_timing_t *timing, double target_fps);
int flip_timing_record_frame(flip_timing_t *timing, double *measured_fps);
int flip_timing_wait_frame(flip_timing_t *timing);
int flip_timing_resync(flip_timing_t *timing);
```

Return semantics:

- `flip_timing_init`: `0` success, `-1` invalid input/system clock failure.
- `flip_timing_record_frame`: `1` when a new approximately-one-second FPS sample is available, `0` otherwise, `-1` on error. When returning `1`, `*measured_fps` contains the new measurement.
- `flip_timing_wait_frame`: `1` when the schedule had to be resynchronised after being more than one frame late, `0` for normal pacing, `-1` for sleep/clock error.
- `flip_timing_resync`: resets the next-frame deadline and FPS measurement window to the current monotonic time; `0` success, `-1` failure.

## Frame order

Preserve the current host-loop ordering:

```text
poll physical input
translate logical controls
retro_run()
handle logical system actions
record completed frame / update FPS sample
if exit requested: leave loop without sleeping
wait for next absolute frame deadline
```

This keeps START+SELECT exit behaviour close to the current implementation and avoids sleeping after an exit request.

## Late-frame policy

The host advances `next_frame` by exactly one frame period and sleeps until that absolute deadline. After waking, it obtains the current monotonic time and compares it with `next_frame + frame_period`.

If current time is later than that limit, the host sets `next_frame = now` and returns `1`. This preserves the existing Flip_GB policy: one badly late frame restarts the schedule instead of causing an uncontrolled catch-up burst.

## FPS measurement

Each completed emulated frame increments `fps_frames`. Once at least one second has elapsed since `fps_started`, measured FPS is:

```text
fps_frames / elapsed_seconds
```

The host then resets the frame counter and measurement start time. `Flip_GB` keeps responsibility for feeding the resulting value to the Video Host overlay and for emulator-specific logging.

## Explicit resync

`flip_timing_resync()` exists now even though Phase 4A3 does not yet move lifecycle handling. Phase 4A5 can call this after suspend/resume or other long interruptions without duplicating timing internals.

## Testing

Add `tests/test_flip_timing.c` and a native `make test-timing` target.

Tests cover:

- invalid FPS rejection
- 59.7275 FPS period calculation (approximately `16,742,706` ns)
- FPS measurement state and output
- forced-late deadline causes schedule resync
- explicit resync resets deadline/measurement state

The full ARM64 build must still pass, followed by real-Miyoo-Flip testing of normal gameplay speed, FPS overlay, GameSwitcher, lid suspend/resume, START+SELECT exit, and audio/control continuity.

## Non-goals

- audio buffering/synchronisation redesign
- dynamic audio-rate control
- frame skipping
- run-ahead
- display refresh-rate matching
- DRM page-flip/vsync policy changes
- performance governor changes
- lifecycle ownership
- core adapter API changes
