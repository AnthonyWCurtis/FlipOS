# FlipOS Phase 4A2 Input Host Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move the proven Miyoo Flip input implementation into the FlipOS runtime layer without changing observable input behaviour.

**Architecture:** Preserve `include/flip_input.h` as the global Input Host API and relocate the existing implementation from `src/hwtest/flip_input.c` to `src/runtime/flip_input.c`. Both frontend and Game Boy emulator continue linking the same `flip_input.o`; no input feature redesign is included.

**Tech Stack:** C, Linux evdev, termios UART, poll, GNU Make, AArch64 cross-toolchain.

**Spec:** `docs/superpowers/specs/2026-08-25-phase4-input-host-design.md`

## Global Constraints

- Work only on `phase4/input-host`.
- Keep `main` at the Phase 4A1 known-good baseline until physical hardware validation succeeds.
- Preserve the current digital mappings exactly.
- Preserve `/dev/ttyS1` UART format `FF YL XL YR XR FE` and 60-frame centre calibration exactly.
- Preserve hall semantics: `/sys/devices/platform/hall-mh248/hallvalue` is authoritative persistent lid state; `/dev/input/event1` is a momentary wake/change pulse.
- Do not add remapping, deadzones, controller profiles, SDL input, `/dev/miyooio`, filtering, power-button integration, or hotkey redesign.
- Do not modify internal NAND/MTD.

---

### Task 1: Relocate the production Input Host implementation

**Files:**
- Create: `src/runtime/flip_input.c`
- Delete: `src/hwtest/flip_input.c`
- Preserve: `include/flip_input.h`

**Interfaces:**
- Consumes: `flip_input_state_t`, `flip_button_t` from `include/flip_input.h`.
- Produces: `flip_input_open(void)`, `flip_input_close(void)`, `flip_input_poll(flip_input_state_t *, int)`, `flip_button_name(flip_button_t)` with unchanged signatures and behaviour.

- [ ] **Step 1: Copy the proven implementation unchanged**

Create `src/runtime/flip_input.c` with the exact contents of the Phase 4A1 `src/hwtest/flip_input.c` implementation.

- [ ] **Step 2: Verify behavioural identity**

Compare old and new source content before deleting the old path. Expected: no code differences; only repository path/ownership changes.

- [ ] **Step 3: Remove the obsolete hardware-test implementation**

Delete `src/hwtest/flip_input.c` after the runtime copy exists.

- [ ] **Step 4: Review the public API**

Confirm `include/flip_input.h` is unchanged. Expected: no API, enum, state-layout, mapping, calibration, hall, or polling changes.

---

### Task 2: Point the build at the runtime Input Host

**Files:**
- Modify: `Makefile`

**Interfaces:**
- Consumes: `src/runtime/flip_input.c`.
- Produces: the existing `build/flip_input.o` used by `flip-frontend` and `flip-gb`.

- [ ] **Step 1: Change only the input object source rule**

Replace:

```make
$(BUILD)/flip_input.o: src/hwtest/flip_input.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
```

with:

```make
$(BUILD)/flip_input.o: src/runtime/flip_input.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
```

- [ ] **Step 2: Inspect the Makefile diff**

Expected: the only Makefile change is `src/hwtest/flip_input.c` -> `src/runtime/flip_input.c` for `flip_input.o`.

---

### Task 3: Source-level verification

**Files:**
- Inspect: `src/runtime/flip_input.c`
- Inspect: `include/flip_input.h`
- Inspect: `Makefile`
- Inspect consumers: `src/frontend/flip_frontend.c`, `src/emulators/flip_gb.c`, `src/runtime/flip_hotkeys.c`

**Interfaces:**
- Consumes: existing callers of the Input Host API.
- Produces: confidence that 4A2 is an ownership extraction only.

- [ ] **Step 1: Confirm caller signatures still match**

Verify all current callers still use `flip_input_open()`, `flip_input_poll()`, `flip_input_close()`, and `flip_input_state_t` without required edits.

- [ ] **Step 2: Confirm no unrelated source changes**

Compare `main` to `phase4/input-host`. Expected functional code delta: runtime input file added, hwtest input file removed, one Makefile source path changed. Documentation commits are allowed.

- [ ] **Step 3: Build locally in WSL**

Run:

```bash
cd ~/flipos-dev
git pull --ff-only
make clean
make all
```

Expected: both `build/flip-frontend` and `build/flip-gb` build successfully with no new compiler errors.

---

### Task 4: Miyoo Flip hardware validation

**Files:**
- No source changes during the first validation pass.

**Interfaces:**
- Consumes: built 4A2 binaries on the development SD card.
- Produces: hardware evidence required for the merge gate.

- [ ] **Step 1: Test frontend digital controls**

Verify Up, Down, Left, Right, A, B, X, Y, L1, R1, L2, R2, Start, Select, Menu, L3, R3, Volume Up, and Volume Down. Expected: identical response to Phase 4A1 with no stuck or missed inputs.

- [ ] **Step 2: Test global hotkeys**

Verify Volume Up/Down and Menu+Volume brightness controls. Expected: identical behaviour to Phase 4A1.

- [ ] **Step 3: Test analogue sticks**

Verify both left and right sticks initialize and report movement normally after the existing 60-frame centre calibration. Expected: no axis-order, direction, or readiness regression.

- [ ] **Step 4: Test Game Boy gameplay input**

Launch a known working Game Boy title and verify D-pad, A, B, Start, Select, Menu/GameSwitcher path, and repeated button presses. Expected: one coherent input snapshot per emulated frame and no regression.

- [ ] **Step 5: Test lid suspend/resume**

Close the lid during gameplay, allow the existing suspend path to complete, reopen it, and verify restored gameplay input. Repeat from frontend. Expected: lid state and wake behaviour remain unchanged and controls work normally after resume.

- [ ] **Step 6: Check unrelated regressions**

Verify video presentation and audio/resume behaviour are no worse than the Phase 4A1 baseline. Expected: no regression caused by the input extraction.

---

### Task 5: Merge gate

**Files:**
- No source changes unless hardware testing finds a confirmed regression.

**Interfaces:**
- Consumes: build result and physical Miyoo Flip validation evidence.
- Produces: decision whether Phase 4A2 may become a known-good baseline.

- [ ] **Step 1: Record hardware result**

Record pass/fail details for digital controls, analogue sticks, hotkeys, gameplay, and lid resume.

- [ ] **Step 2: Merge only after a full pass**

If all checks pass, Phase 4A2 can be marked known-good and merged into `main`. If any check fails, keep `main` unchanged and debug only on `phase4/input-host`.
