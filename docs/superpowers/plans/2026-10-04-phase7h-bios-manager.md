# Phase 7H BIOS Manager Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a formal BIOS Manager that owns the existing GB/GBC system directory and reports structural BIOS health without enabling BIOS execution.

**Architecture:** Add a small runtime service for canonical BIOS metadata, path construction, and exact-size validation. Gambatte keeps identical runtime behavior but obtains `RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY` from BIOS Manager instead of a hard-coded path. No core option enables the official bootloader in this phase.

**Tech Stack:** C11 runtime service, existing FlipOS path service, Libretro Gambatte adapter, host C tests, Python architecture guards, Makefile verification.

**Spec:** `docs/superpowers/specs/2026-10-04-phase7h-bios-manager-design.md`

## Global Constraints

- Preserve `/mnt/SDCARD/flipos-dev/system/gb` exactly.
- GB BIOS filename `gb_bios.bin`, expected size 256 bytes.
- GBC BIOS filename `gbc_bios.bin`, expected size 2304 bytes.
- Status values: `MISSING / VALID / INVALID / ERROR`.
- Do not ship, copy, download, generate, or enable BIOS files.
- Do not enable `gambatte_gb_bootloader`.
- BIOS status must never block game startup.
- No NAND/MTD or `/usr/miyoo` changes.
- Add behavioral and architecture tests to `make verify`.

## Review Focus

- Unknown system IDs must return an error rather than silently choosing a directory.
- Invalid BIOS kind values must fail safely.
- A directory at a BIOS filename must report INVALID.
- Wrong-size regular files must report INVALID with actual size populated.
- Gambatte integration must preserve the exact existing system-directory string and leave bootloader activation absent.

---

### Task 1: Canonical BIOS paths and validation service

**Files:**
- Modify: `include/flip_paths.h`
- Modify: `src/runtime/flip_paths.c`
- Create: `include/flip_bios.h`
- Create: `src/runtime/flip_bios.c`
- Create: `tests/test_flip_bios.c`
- Modify: `Makefile`

**Interfaces:**
- Produces: `const char *flip_paths_system_root(void)`
- Produces: `int flip_bios_system_directory(const char *system_id, char *path, size_t path_size)`
- Produces: `int flip_bios_query(flip_bios_kind_t kind, flip_bios_info_t *info)`

- [ ] **Step 1: Write failing BIOS Manager behavioral tests**

Assert:
- `GB` system directory resolves to `<test-root>/gb`.
- unknown system ID fails.
- missing DMG/GBC files report `FLIP_BIOS_MISSING`.
- exact 256-byte DMG file reports VALID.
- exact 2304-byte CGB file reports VALID.
- short/oversized files report INVALID and expose actual size.
- directory at BIOS path reports INVALID.
- invalid BIOS kind and null outputs fail safely.

- [ ] **Step 2: Run the BIOS test and confirm RED**

Run: `make test-bios`

Expected: compile failure because `flip_bios.h` / BIOS APIs do not exist.

- [ ] **Step 3: Add canonical system root**

Add `FLIP_SYSTEM_ROOT` as `FLIP_STORAGE_ROOT "/system"` and `flip_paths_system_root()`.

- [ ] **Step 4: Implement BIOS Manager**

Define:
- `flip_bios_kind_t`: `FLIP_BIOS_GB_DMG`, `FLIP_BIOS_GB_CGB`
- `flip_bios_status_t`: `MISSING`, `VALID`, `INVALID`, `ERROR`
- `FLIP_BIOS_PATH_MAX`
- `flip_bios_info_t`
- `flip_bios_system_directory()`
- `flip_bios_query()`

Use exact filename/size metadata and `stat()`; never create firmware files.

- [ ] **Step 5: Run focused tests GREEN**

Run: `make test-paths test-bios`

Expected: both pass.

- [ ] **Step 6: Commit**

Commit service + behavioral tests as one independently reviewable unit.

### Task 2: Gambatte ownership integration and architecture guard

**Files:**
- Modify: `src/emulators/flip_gambatte_core.c`
- Create: `tests/test_bios_architecture.py`
- Modify: `Makefile`

**Interfaces:**
- Consumes: `flip_bios_system_directory("GB", ...)`
- Produces: unchanged Libretro system-directory behavior.

- [ ] **Step 1: Write failing architecture guard**

Require:
- Gambatte includes/uses BIOS Manager.
- Gambatte contains no `gb_bios.bin`, `gbc_bios.bin`, or hard-coded `/mnt/SDCARD/flipos-dev/system/gb`.
- `gambatte_gb_bootloader` is not enabled by FlipOS.
- `test-bios` and BIOS architecture guard are wired into `make verify`.

- [ ] **Step 2: Run architecture guard RED**

Run: `python3 tests/test_bios_architecture.py`

Expected: fail on current hard-coded system directory / missing BIOS Manager use.

- [ ] **Step 3: Replace hard-coded system directory**

Make the Gambatte environment callback obtain the canonical `GB` system directory from BIOS Manager while preserving the exact effective path.

The path buffer must have lifetime suitable for Libretro environment callbacks.

- [ ] **Step 4: Wire build and verification**

Compile `src/runtime/flip_bios.c` into targets that need it; add `test-bios` and `test_bios_architecture.py` to the standard verify gate and clean target.

- [ ] **Step 5: Run focused integration tests GREEN**

Run:
- `make test-bios`
- `python3 tests/test_bios_architecture.py`

Expected: pass.

- [ ] **Step 6: Commit**

Commit Gambatte integration + architecture guard.

### Task 3: Full verification, deployment, and hardware acceptance

**Files:**
- Create after acceptance: `docs/superpowers/specs/2026-10-04-phase7h-bios-manager-acceptance.md`
- Modify after acceptance: `docs/ROADMAP.md`

**Interfaces:**
- Consumes the completed BIOS Manager and unchanged Gambatte bootloader behavior.
- Produces hardware-accepted Phase 7H merge candidate.

- [ ] **Step 1: Run final host verification**

Run:
- `git diff --check`
- `make clean && make verify`
- `sha256sum build/flip-frontend build/flip-gb`

Expected: `VERIFY: PASS`.

- [ ] **Step 2: Create/push feature branch and draft PR**

Preserve exact verified commit SHA and binary hashes.

- [ ] **Step 3: Stage hardware binaries with rollback**

Preserve current Phase 7G accepted binaries, push changed binaries side-by-side, verify hashes, replace only changed binaries, reboot.

- [ ] **Step 4: Verify BIOS status with empty real directory**

Expected:
- DMG -> MISSING
- CGB -> MISSING
- game startup remains allowed.

- [ ] **Step 5: Verify no BIOS execution**

Confirm no Gambatte bootloader-read log and no new bootloader option enablement.

- [ ] **Step 6: Run gameplay regression**

Resume Red:
- battery save loads
- state restores
- MENU suspend saves and returns to GameSwitcher.

- [ ] **Step 7: Run physical lid regression**

Close/open lid and verify deep suspend, wake, RK817 recovery, automatic resume, and state restore unchanged.

- [ ] **Step 8: Leave Flip quiet and record acceptance**

Return to GameSwitcher, write acceptance doc, mark BIOS Manager complete in roadmap.

- [ ] **Step 9: Merge accepted PR and sync `main`**

Merge only the exact hardware-accepted head and fast-forward local `main`.
