# Phase 7H — BIOS Manager Design

Status: design proposed

## Intent

Introduce a formal BIOS Manager that discovers and validates user-supplied firmware files on the SD card, exposes their health to emulator adapters and future UI, and centralizes BIOS path ownership.

Phase 7H must not change current Game Boy / Game Boy Color boot behavior. Valid BIOS files may be discovered and reported, but Gambatte's official bootloader option remains disabled until a later explicitly approved feature enables it.

## Existing behavior

FlipOS currently exposes Gambatte's Libretro system directory as:

`/mnt/SDCARD/flipos-dev/system/gb`

Gambatte already knows these filenames:

- `gb_bios.bin` — 256 bytes
- `gbc_bios.bin` — 2304 bytes

Gambatte only uses those files when `gambatte_gb_bootloader` is enabled. FlipOS currently supplies no enabled value for that option, so official BIOS boot is disabled.

The directory is therefore an implicit BIOS location today, but there is no FlipOS service that owns the path, validates files, or reports firmware health.

## Goals

Phase 7H will:

1. Create a dedicated BIOS Manager runtime service.
2. Preserve the existing GB system directory exactly.
3. Discover the expected GB and GBC BIOS filenames.
4. Validate each file structurally using exact expected size.
5. Report `MISSING / VALID / INVALID / ERROR`.
6. Expose canonical system-directory lookup for emulator adapters.
7. Make Gambatte obtain its Libretro system directory from BIOS Manager instead of a hard-coded path.
8. Add unit and architecture tests.
9. Preserve all current ROM, save, state, session, audio, power, and boot behavior.

## Non-goals

Phase 7H will not:

- ship, download, copy, generate, or redistribute BIOS files
- automatically enable Gambatte official BIOS boot
- require BIOS files for games to run
- migrate existing files
- add a BIOS settings screen
- change save/state formats or paths
- cryptographically validate BIOS contents yet

A later phase can add explicit BIOS activation once discovery and validation are proven.

## Architecture

Add:

- `include/flip_bios.h`
- `src/runtime/flip_bios.c`

The service owns BIOS metadata and path policy.

Phase 7H defines two BIOS kinds:

- DMG / GB: `gb_bios.bin`, exact size 256 bytes
- CGB / GBC: `gbc_bios.bin`, exact size 2304 bytes

Both remain under the existing GB system directory because Gambatte already expects them there.

Proposed public contract:

```c
typedef enum {
    FLIP_BIOS_GB_DMG = 0,
    FLIP_BIOS_GB_CGB
} flip_bios_kind_t;

typedef enum {
    FLIP_BIOS_MISSING = 0,
    FLIP_BIOS_VALID,
    FLIP_BIOS_INVALID,
    FLIP_BIOS_ERROR
} flip_bios_status_t;

typedef struct {
    flip_bios_kind_t kind;
    flip_bios_status_t status;
    const char *system_id;
    const char *filename;
    size_t expected_size;
    size_t actual_size;
    char path[FLIP_BIOS_PATH_MAX];
} flip_bios_info_t;

int flip_bios_system_directory(
    const char *system_id,
    char *path,
    size_t path_size);

int flip_bios_query(
    flip_bios_kind_t kind,
    flip_bios_info_t *info);
```

Exact names may vary slightly during implementation, but ownership and behavior must remain equivalent.

## Path ownership

BIOS Manager derives paths from canonical FlipOS storage definitions rather than embedding absolute strings in emulator code.

The effective path must remain:

`/mnt/SDCARD/flipos-dev/system/gb`

If `flip_paths.h` needs a canonical system-directory root, Phase 7H may add one as long as the on-disk path does not change.

The service may ensure directories exist when appropriate, but it must never create BIOS files.

## Validation rules

For each known BIOS:

1. Build the canonical path.
2. `stat()` the path.
3. Missing file -> `MISSING`.
4. Existing non-regular file -> `INVALID`.
5. Wrong exact size -> `INVALID`.
6. Other metadata/access failure -> `ERROR`.
7. Correct regular file and size -> `VALID`.

Phase 7H intentionally uses exact-size structural validation only.

No hash whitelist is required in this phase because actual BIOS execution is out of scope and a premature hash policy could reject legitimate dumps.

## Gambatte integration

`src/emulators/flip_gambatte_core.c` currently hard-codes:

`/mnt/SDCARD/flipos-dev/system/gb`

for `RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY`.

Phase 7H replaces that hard-coded ownership with BIOS Manager lookup while returning exactly the same effective directory.

FlipOS must continue leaving `gambatte_gb_bootloader` disabled.

Therefore:

- valid BIOS present -> discovered/reported, not executed
- missing BIOS -> games continue as today
- invalid BIOS -> games continue as today

This preservation is the central compatibility guarantee.

## Error handling and logging

BIOS validation status is informational in Phase 7H and must never block game startup.

The service should log deterministic status such as:

`BIOS: kind=GB_DMG status=MISSING expected=256 actual=0 path=...`

`BIOS: kind=GB_CGB status=VALID expected=2304 actual=2304 path=...`

Logs must not imply a valid BIOS is active in Gambatte.

## Testing

### Unit tests

Cover:

- missing GB BIOS
- missing GBC BIOS
- valid exact-size GB file
- valid exact-size GBC file
- undersized file
- oversized file
- directory at BIOS path
- canonical system-directory resolution
- no mutation of BIOS file contents

Use temporary host roots through compile-time path overrides or existing path-test patterns.

### Architecture tests

Require:

- BIOS metadata/path policy lives in BIOS Manager
- Gambatte adapter gets system directory through BIOS Manager
- no BIOS filename ownership in Gambatte adapter
- no hard-coded `/mnt/SDCARD/flipos-dev/system/gb` in Gambatte adapter
- no bootloader enablement added in Phase 7H

BIOS Manager behavioral and architecture tests must be wired into `make verify`.

## Hardware acceptance

Required checks:

1. Boot exact staged Phase 7H hashes.
2. Confirm settings and sessions remain intact.
3. Query BIOS Manager on the real SD card with no BIOS files:
   - GB -> MISSING
   - GBC -> MISSING
4. Confirm Red resumes with its existing save/state.
5. Confirm Gambatte does not log that a bootloader was read.
6. Confirm MENU suspend still saves and returns to GameSwitcher.
7. Perform one real lid close/open cycle.
8. Confirm automatic resume, RK817 recovery, and state restore remain unchanged.
9. Return the Flip to GameSwitcher and leave it quiet.

User-supplied BIOS files are not required for Phase 7H acceptance.

## Rollback

Use the established Phase 7 deployment discipline:

- preserve currently accepted binaries
- stage replacements side-by-side
- verify hashes before replacement
- replace only binaries that changed
- reboot
- restore previous accepted binaries if hardware acceptance fails

No NAND/MTD or `/usr/miyoo` changes are permitted.

## Future extension

Later explicitly approved work may add:

- BIOS inventory UI
- known-good hash identification
- per-core BIOS requirements
- explicit user setting to enable official boot ROMs
- warnings for configured but missing/invalid BIOS files

Those features are intentionally excluded from Phase 7H.
