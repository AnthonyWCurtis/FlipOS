# Phase 4A2B Logical Controls Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Separate normalized game controls from FlipOS system actions so `flip-gb` no longer interprets Miyoo-specific physical shortcut buttons directly.

**Architecture:** Keep `flip_input` unchanged as the physical hardware acquisition layer. Add pure `flip_controls` translation state, feed normalized game controls to Gambatte, and feed logical system actions to shared host services and the current emulator lifecycle code.

**Tech Stack:** C11, Linux evdev/UART input through existing `flip_input`, libretro Gambatte host, GNU Make, host GCC unit test, AArch64 cross-build for device binaries.

**Spec:** `docs/superpowers/specs/2026-08-25-phase4-input-actions-design.md`

## Global Constraints

- Do not modify internal NAND/MTD.
- Keep `main` known-good; implementation stays on `phase4/input-actions` until build and real-hardware validation pass.
- Preserve existing physical input mapping and all currently verified user-visible behavior.
- Do not start Timing, Audio, Lifecycle, State Manager or additional-emulator work in this milestone.
- START+SELECT is a global FlipOS exit shortcut.

---

### Task 1: Add pure logical controls API and unit tests

**Files:**
- Create: `include/flip_controls.h`
- Create: `src/runtime/flip_controls.c`
- Create: `tests/test_flip_controls.c`
- Modify: `Makefile`

**Interfaces:**
- Consumes: `flip_input_state_t` from `include/flip_input.h`
- Produces: `flip_controls_init()` and `flip_controls_update()` plus `flip_game_input_t` and `flip_system_actions_t`

- [ ] **Step 1: Add the host-runnable failing unit test**

The test must cover game mapping, analogue centre-relative values, MENU-alone, MENU+R1, START+SELECT, volume, MENU+volume brightness, and lid-close suspend semantics.

- [ ] **Step 2: Run the unit test before implementation**

Run:

```bash
gcc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_flip_controls.c -o build/test-flip-controls
```

Expected: FAIL because `flip_controls.h` / implementation does not yet exist.

- [ ] **Step 3: Implement the pure controls transform**

Create the API and implementation described in the design. The transform must perform no device I/O and must emit one-update action pulses using previous-state tracking.

- [ ] **Step 4: Add a Makefile host test target and runtime object rule**

Add `HOSTCC ?= gcc`, compile `src/runtime/flip_controls.c` into normal runtime builds, and add `make test-controls` that builds and executes the host unit test.

- [ ] **Step 5: Run the unit test**

Run:

```bash
make test-controls
```

Expected: `flip_controls tests passed` and exit 0.

- [ ] **Step 6: Commit**

```bash
git add include/flip_controls.h src/runtime/flip_controls.c tests/test_flip_controls.c Makefile
git commit -m "Add logical controls and system actions"
```

### Task 2: Add action-based hotkey execution

**Files:**
- Modify: `include/flip_hotkeys.h`
- Modify: `src/runtime/flip_hotkeys.c`

**Interfaces:**
- Consumes: `flip_system_actions_t`
- Produces: `flip_hotkeys_apply_actions()`
- Preserves: existing `flip_hotkeys_handle()` for frontend compatibility

- [ ] **Step 1: Route volume and brightness effects through a shared action executor**

`flip_hotkeys_apply_actions()` must service delayed settings persistence, apply ±5 volume steps, apply ±20 brightness steps with existing clamps, show existing OSD feedback, and populate `flip_hotkeys_result_t`.

- [ ] **Step 2: Keep the legacy frontend entry point behavior-compatible**

`flip_hotkeys_handle()` must continue accepting a physical input snapshot, edge-detecting volume keys exactly as before, translating that into logical steps, and delegating the actual effect to the new action executor.

- [ ] **Step 3: Run controls tests**

```bash
make test-controls
```

Expected: PASS.

- [ ] **Step 4: Commit**

```bash
git add include/flip_hotkeys.h src/runtime/flip_hotkeys.c
git commit -m "Apply global hotkeys from logical actions"
```

### Task 3: Migrate Flip_GB to logical controls

**Files:**
- Modify: `src/emulators/flip_gb.c`

**Interfaces:**
- Consumes: `flip_controls_update()` and `flip_hotkeys_apply_actions()`
- Produces: Gambatte joypad callbacks reading only normalized game controls; lifecycle decisions reading only logical system actions

- [ ] **Step 1: Replace emulator-visible physical state with logical game state**

Keep a physical snapshot only as input to shared host services. Add `flip_controls_t` and `flip_controls_state_t`. Gambatte's `input_state_cb()` must read only `control_state.game`.

- [ ] **Step 2: Replace physical shortcut interpretation**

Remove direct `FLIP_BTN_MENU`, `FLIP_BTN_R1`, `FLIP_BTN_START`, `FLIP_BTN_SELECT`, lid transition bookkeeping and menu-pending bookkeeping from `flip-gb`. Use logical action fields instead while preserving the existing ordering after `retro_run()`.

- [ ] **Step 3: Route volume/brightness through action-based hotkeys**

Call `flip_hotkeys_apply_actions()` with `control_state.system`.

- [ ] **Step 4: Static verification**

Run:

```bash
grep -n "FLIP_BTN_" src/emulators/flip_gb.c
```

Expected: no output.

- [ ] **Step 5: Run unit tests and full build**

```bash
make test-controls
make clean
make all
```

Expected: controls tests pass; `build/flip-frontend` and `build/flip-gb` are produced successfully. Existing unrelated Gambatte warnings may remain; no new build errors are acceptable.

- [ ] **Step 6: Commit**

```bash
git add src/emulators/flip_gb.c
git commit -m "Move Flip_GB to logical input actions"
```

### Task 4: Hardware acceptance and roadmap update

**Files:**
- Modify after hardware pass: `docs/ROADMAP.md`

- [ ] **Step 1: Deploy only runtime payload**

Deploy `.tmp_update/updater`, `flipos-dev/bin/flip-frontend`, and `flipos-dev/bin/flip-gb` to the FAT32 SD card.

- [ ] **Step 2: Verify on the real Miyoo Flip**

Required checks:

- D-pad, A/B, Start/Select gameplay
- MENU alone opens GameSwitcher
- MENU+R1 toggles FPS once and does not also open GameSwitcher
- volume up/down
- MENU+volume brightness changes and does not also open GameSwitcher
- START+SELECT exits cleanly
- lid close saves/suspends; lid open resumes
- input works immediately after resume
- audio recovery remains functional
- frontend navigation and global controls are unchanged

- [ ] **Step 3: Mark Phase 4A2B complete only after hardware evidence**

Update `docs/ROADMAP.md`, fast-forward `main` only after all checks pass, and preserve the feature branch history.
