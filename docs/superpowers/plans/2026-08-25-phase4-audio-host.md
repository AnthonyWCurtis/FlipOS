# Phase 4A4 Global Audio Host Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move Gambatte PCM delivery behind a shared, bounded, synchronous FlipOS Audio Host while preserving the hardware-proven RK817/aplay transport.

**Architecture:** A private 256-frame stereo staging helper receives core PCM and flushes synchronously through a sink callback. `flip_audio.c` owns the physical transport and RK817 behavior, while `flip_gb.c` only supplies the core-reported audio configuration, submits samples, and ends each emulated audio frame.

**Tech Stack:** C11, POSIX pipe/fork/write/wait, Linux I2C RK817 access, existing aplay/ALSA userspace transport, GNU Make, native host gcc tests, AArch64 cross compiler.

**Spec:** `docs/superpowers/specs/2026-08-25-phase4-audio-host-design.md`

## Global Constraints

- Development remains SD-card-only; do not modify NAND/MTD.
- Preserve the existing RK817 exact-signature recovery behavior.
- Preserve the existing aplay transport and parameters in 4A4.
- Accept only S16_LE stereo at 32768 Hz in this milestone.
- Do not add libasound, audio threads, a large ring buffer, resampling, DSP, or dynamic A/V drift correction.
- Timing Host remains the emulation/video clock master.
- No FlipOS-owned audio may remain staged across an emulated frame boundary after `flip_audio_end_frame()`.
- `main` remains the hardware-tested baseline until real-device acceptance passes.

---

### Task 1: Native-testable bounded audio staging

**Files:**
- Create: `src/audio/flip_audio_buffer.h`
- Create: `src/audio/flip_audio_buffer.c`
- Create: `tests/test_flip_audio_buffer.c`
- Modify: `Makefile`

**Interfaces:**
- Produces internal `flip_audio_buffer_t` and sink-callback API used by `flip_audio.c`.
- Fixed stereo capacity: 256 frames.

```c
typedef int (*flip_audio_buffer_sink_fn)(
    const int16_t *samples,
    size_t frames,
    void *userdata);

typedef struct {
    int16_t samples[256 * 2];
    size_t frames;
} flip_audio_buffer_t;

void flip_audio_buffer_init(flip_audio_buffer_t *buffer);
size_t flip_audio_buffer_submit(
    flip_audio_buffer_t *buffer,
    const int16_t *samples,
    size_t frames,
    flip_audio_buffer_sink_fn sink,
    void *userdata,
    int *sink_failed);
int flip_audio_buffer_flush(
    flip_audio_buffer_t *buffer,
    flip_audio_buffer_sink_fn sink,
    void *userdata);
void flip_audio_buffer_reset(flip_audio_buffer_t *buffer);
```

- [ ] **Step 1: Write failing native tests**

Test these exact behaviors:

```c
/* 100 frames remain staged until explicit flush. */
assert(flip_audio_buffer_submit(&buffer, source, 100, sink, &capture, &failed) == 100);
assert(capture.frames == 0);
assert(buffer.frames == 100);
assert(flip_audio_buffer_flush(&buffer, sink, &capture) == 0);
assert(capture.frames == 100);
assert(buffer.frames == 0);

/* 300 frames flush first 256 synchronously and retain 44. */
assert(flip_audio_buffer_submit(&buffer, source, 300, sink, &capture, &failed) == 300);
assert(capture.frames == 256);
assert(buffer.frames == 44);
assert(flip_audio_buffer_flush(&buffer, sink, &capture) == 0);
assert(capture.frames == 300);

/* Sink failure is surfaced and no additional input is accepted. */
capture.fail = 1;
assert(flip_audio_buffer_submit(&buffer, source, 300, sink, &capture, &failed) < 300);
assert(failed == 1);
```

- [ ] **Step 2: Verify RED**

Run:

```bash
make test-audio
```

Expected: fail because `flip_audio_buffer` implementation/target does not yet exist.

- [ ] **Step 3: Implement the minimal fixed-capacity buffer**

Copy stereo PCM in order, flush only when full or explicitly requested, and stop accepting new frames after a sink failure in the current submit call.

- [ ] **Step 4: Verify GREEN**

Run:

```bash
make test-audio
```

Expected:

```text
flip_audio_buffer tests passed
```

- [ ] **Step 5: Re-run existing host tests**

```bash
make test-controls
make test-timing
```

Expected: both pass.

### Task 2: Public Audio Host configuration and transport integration

**Files:**
- Modify: `include/flip_audio.h`
- Modify: `src/audio/flip_audio.c`
- Uses: `src/audio/flip_audio_buffer.h`

**Interfaces:**

```c
typedef struct {
    unsigned sample_rate;
    unsigned channels;
} flip_audio_config_t;

int flip_audio_open(const flip_audio_config_t *config);
size_t flip_audio_submit(const int16_t *samples, size_t frames);
int flip_audio_end_frame(void);
void flip_audio_close(void);
```

Existing volume functions remain unchanged.

- [ ] **Step 1: Validate configuration before hardware access**

`flip_audio_open()` returns `-1` unless:

```text
sample_rate == 32768
channels == 2
```

- [ ] **Step 2: Preserve the proven transport**

Retain exactly:

```text
aplay -q -D hw:0,0 -t raw -f S16_LE -c 2 -r 32768 -B 50000 -F 10000
```

Retain SIGPIPE ignore, EINTR retry, EPIPE handling, mixer setup, and RK817 exact-signature recovery.

- [ ] **Step 3: Route staging-buffer sink to the aplay pipe**

The sink writes every requested byte synchronously. On transport failure it marks the pipe unavailable and returns `-1`.

- [ ] **Step 4: Implement submit/end-frame**

`flip_audio_submit()` forwards through the staging helper. On sink failure, reset buffered data and continue the emulator silently.

`flip_audio_end_frame()` flushes staged frames. If audio is unopened/unavailable it is a safe no-op.

- [ ] **Step 5: Flush before close**

If the transport remains live, flush pending staged frames before closing the pipe and waiting for aplay.

### Task 3: Flip_GB Audio Host integration

**Files:**
- Modify: `src/emulators/flip_gb.c`

**Interfaces consumed:**
- `flip_audio_open(const flip_audio_config_t *)`
- `flip_audio_submit(const int16_t *, size_t)`
- `flip_audio_end_frame()`

- [ ] **Step 1: Move audio open after AV-info discovery**

Remove the current early `flip_audio_open()` call. After `retro_get_system_av_info(&av)`, configure:

```c
flip_audio_config_t audio_config = {
    .sample_rate = (unsigned)(av.timing.sample_rate + 0.5),
    .channels = 2,
};
```

Then open audio. Failure retains the current `continuing silent` behavior.

- [ ] **Step 2: Migrate both callbacks**

Single-sample callback:

```c
return-through staging via flip_audio_submit(frame, 1);
```

Batch callback:

```c
return flip_audio_submit(data, frames);
```

- [ ] **Step 3: Establish frame flush boundary**

Immediately after every `retro_run()`:

```c
if (flip_audio_end_frame() < 0)
    fprintf(stderr, "FLIPGB: audio frame flush failed\n");
```

This occurs before lid/GameSwitcher/exit handling.

- [ ] **Step 4: Remove all use of `flip_audio_write`**

```bash
grep -R "flip_audio_write" include src tests || true
```

Expected: no matches.

### Task 4: Build and source verification

**Files:**
- Modify: `Makefile`

- [ ] **Step 1: Link private audio buffer into Flip_GB**

Add `build/flip_audio_buffer.o` to `GB_OBJS` and its compile rule.

- [ ] **Step 2: Add native test target**

`make test-audio` compiles only the pure buffer implementation with host gcc and `-Werror`.

- [ ] **Step 3: Full verification**

Run:

```bash
make clean
make test-controls
make test-timing
make test-audio
make all
git diff --check
git status
```

Expected: all native tests pass and both ARM64 binaries link. Existing third-party Gambatte/libretro warnings are acceptable; new FlipOS warnings are not.

### Task 5: Real Miyoo Flip acceptance and baseline promotion

**Files:**
- Modify after acceptance: `docs/ROADMAP.md`

- [ ] **Step 1: Deploy only changed runtime binaries**

Copy the freshly built `flip-gb` to the SD and verify SHA-256 matches the local build.

- [ ] **Step 2: Hardware acceptance**

Verify:

```text
GB audio starts normally
extended gameplay audio remains approximately synchronized
no new crackles/dropouts
FPS remains approximately 59.7
volume controls work
GameSwitcher -> resume restores audio
lid close/open -> audio recovers
START+SELECT remains correct
```

- [ ] **Step 3: Preserve exact tested source**

Stage only the hardware-tested source changes, commit, and push the feature branch.

- [ ] **Step 4: Mark roadmap and fast-forward main**

Check all Phase 4A4 items only after hardware acceptance, record the hardware-tested code commit, then fast-forward `main` without force.
