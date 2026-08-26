# Phase 4B Common Core Adapter API Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Refactor the existing Gambatte Game Boy runtime so a shared FlipOS Core Host owns the reusable execution loop and Gambatte is accessed only through a core-neutral adapter API, without adding another emulator core.

**Architecture:** Introduce a checked `flip_core_adapter_t` contract and a shared `flip_core_host` loop. Move libretro/Gambatte translation into `flip_gambatte_core.c`; reduce `flip_gb.c` to composition plus the existing temporary GB resume-state persistence. Rename the current process-level emulator adapter to `flip_session_adapter_t` so core and session concepts are unambiguous.

**Tech Stack:** C11, existing static ARM64 toolchain, Gambatte libretro static archive, existing FlipOS Video/Audio/Input/Controls/Timing/Lifecycle hosts.

**Spec:** `docs/superpowers/specs/2026-08-26-phase4-core-adapter-api-design.md`

## Global Constraints

- SD-card-first development only.
- Do not modify internal NAND/MTD.
- Preserve the stock Miyoo recovery path.
- Keep `main` known-good and real-hardware tested.
- Work only on `phase4/core-adapter-api` until hardware validation passes.
- Add no second emulator/core or playable system.
- Do not implement the Phase 5 State Manager.
- Preserve the existing GB state path/hash/file format and frontend exit-code protocol.
- Preserve the established frame order and Video/Input/Timing/Audio/Lifecycle Host behaviour.

---

### Task 1: Core-neutral adapter contract

**Files:**
- Create: `include/flip_core.h`
- Create: `src/runtime/flip_core.c`
- Create: `tests/test_flip_core.c`
- Modify: `Makefile`

**Interfaces:**
- Produces `flip_core_info_t`, `flip_core_adapter_t`, and checked wrappers: `flip_core_init`, `flip_core_deinit`, `flip_core_load_game`, `flip_core_unload_game`, `flip_core_get_info`, `flip_core_set_input`, `flip_core_run_frame`, `flip_core_serialize_size`, `flip_core_serialize`, `flip_core_unserialize`.
- The public header contains no libretro types.

- [ ] Write tests first for a complete fake adapter, missing required operations, call forwarding, input forwarding, and serialization forwarding.
- [ ] Run the host test and verify RED because `flip_core.h`/`flip_core.c` do not yet exist.
- [ ] Implement the smallest checked wrapper layer that makes the tests pass.
- [ ] Run `make test-core`; require clean `-Werror` GREEN.
- [ ] Commit the contract, implementation, tests, and Makefile target.

### Task 2: Rename process/session adapter

**Files:**
- Create: `include/flip_session_adapter.h`
- Create: `src/runtime/flip_session_adapter.c`
- Modify: `include/flip_system.h`
- Modify: `src/runtime/flip_system.c`
- Modify: `src/runtime/flip_session.c`
- Modify: `Makefile`
- Delete after migration: `include/flip_emulator_adapter.h`, `src/runtime/flip_emulator_adapter.c`

**Interfaces:**
- `flip_session_adapter_t` retains exactly `map_exit_code` and `resume_available` semantics.
- `flip_session_adapter_for_system()` replaces `flip_emulator_adapter_for_system()`.
- `flip_system_t.session_adapter` replaces `flip_system_t.adapter`.

- [ ] Add/extend native tests that prove exit 20 still maps to suspend, other exit codes map to exit, and existing GB resume-state availability remains unchanged.
- [ ] Verify RED against the old names.
- [ ] Perform a mechanical naming migration with no policy changes.
- [ ] Run the relevant native tests and full host compile checks.
- [ ] Commit the naming cleanup separately.

### Task 3: Shared Core Host orchestration

**Files:**
- Create: `include/flip_core_host.h`
- Create: `src/runtime/flip_core_host.c`
- Create: `tests/test_flip_core_host.c`
- Modify: `Makefile`

**Interfaces:**
- Consumes a `flip_core_adapter_t`, ROM path, a save callback/userdata, and a restored-state flag.
- Produces a process exit code selected by Lifecycle Host policy.
- Preserves frame order: input poll -> controls update -> adapter input -> one adapter frame -> audio end-frame -> immediate lifecycle -> FPS/hotkeys -> timing accounting -> after-frame lifecycle -> timing wait.

- [ ] Write fake-core host tests first for one-frame ordering, normalized input-before-frame, lifecycle exit-code propagation, and resume timing resync only when `state_restored` is true.
- [ ] Verify RED because the host does not exist.
- [ ] Implement only enough orchestration to satisfy the tests while calling existing shared hosts.
- [ ] Run `make test-core-host`; require clean GREEN.
- [ ] Commit Core Host and tests.

### Task 4: Gambatte core adapter

**Files:**
- Create: `include/flip_gambatte_core.h`
- Create: `src/emulators/flip_gambatte_core.c`
- Modify: `Makefile`

**Interfaces:**
- Produces `flip_core_adapter_t *flip_gambatte_core_adapter(void)` or equivalent stable accessor.
- Owns libretro environment/video/audio/input callbacks, ROM loading, `retro_init/deinit`, `retro_load/unload_game`, `retro_run`, and raw serialize/unserialize forwarding.
- Accepts only normalized `flip_game_input_t`; contains no Miyoo physical-button knowledge.

- [ ] Write adapter-facing tests where practical around pure translation/contract behaviour before implementation.
- [ ] Verify RED for the missing Gambatte adapter surface.
- [ ] Move existing libretro integration out of `flip_gb.c` without changing callback semantics.
- [ ] Build the ARM64 adapter object and link against the existing Gambatte archive.
- [ ] Commit Gambatte adapter extraction.

### Task 5: Reduce `flip-gb` to composition and temporary persistence

**Files:**
- Modify: `src/emulators/flip_gb.c`
- Modify: `Makefile`
- Optional guarded migration helper: `tools/apply_phase4b_core_host.py`

**Interfaces:**
- `flip_gb.c` validates argv, keeps the current GB state path/hash/read/write behaviour, configures the Gambatte adapter, restores through neutral `flip_core_unserialize`, invokes `flip_core_host_run`, and returns its selected exit code.
- Lifecycle saves call neutral `flip_core_serialize` through the existing temporary GB persistence callback.

- [ ] Add source-level/contract checks that `flip_gb.c` no longer calls `retro_run`, polls FlipOS input, performs timing waits, or translates libretro joypad IDs.
- [ ] Verify those checks fail before migration.
- [ ] Migrate using exact-string guarded edits or a carefully reviewed manual replacement; do not use regex replacement for C blocks.
- [ ] Run all native tests and the full ARM64 static build.
- [ ] Verify the old GB state path/hash constants and state-file compatibility logic remain unchanged.
- [ ] Commit the exact source used for hardware testing.

### Task 6: Verification and hardware acceptance

**Files:**
- Modify after successful hardware test: `docs/ROADMAP.md`

- [ ] Run `make clean && make test-controls && make test-timing && make test-audio && make test-lifecycle && make test-core && make test-core-host && make all`.
- [ ] Run `git diff --check` and require a clean working tree except the intentional hardware-test migration if staged later.
- [ ] Deploy only `flip-frontend`/`flip-gb` to the development SD; do not touch NAND/MTD.
- [ ] On the real Flip verify existing GB ROM launch, video/audio/input, hotkeys, ~59.7 FPS, GameSwitcher save/resume, title switching, lid suspend/resume, RK817 recovery, START+SELECT exit, and pre-4B resume-state compatibility.
- [ ] Inspect logs for Core Host path and absence of new errors.
- [ ] Commit the exact hardware-tested source before further source changes.
- [ ] Mark 4B hardware-verified in `docs/ROADMAP.md`, preserving the hardware-tested code SHA.
- [ ] Fast-forward `main` only after branch comparison shows 0 behind and hardware acceptance is complete.
