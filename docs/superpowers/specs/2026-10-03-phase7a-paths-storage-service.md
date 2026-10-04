# FlipOS Phase 7A — Paths / Storage Service

Date: 2026-10-03
Status: hardware accepted
Branch: `phase7/paths-storage-service`

## Goal

Establish one canonical owner for FlipOS SD-card storage paths before formalizing the remaining Phase 7 system services.

This milestone is deliberately a refactor. It must not move, rename, delete, migrate, or reinterpret any existing user data.

## Canonical storage contract

`include/flip_paths.h` now owns the default FlipOS layout.

Current defaults remain exactly:

```text
/mnt/SDCARD/flipos-dev
/mnt/SDCARD/flipos-dev/bin
/mnt/SDCARD/flipos-dev/cache
/mnt/SDCARD/flipos-dev/cache/library
/mnt/SDCARD/flipos-dev/saves
/mnt/SDCARD/flipos-dev/states
/mnt/SDCARD/flipos-dev/sessions
/mnt/SDCARD/flipos-dev/state
/mnt/SDCARD/flipos-dev/state/settings
/mnt/SDCARD/flipos-dev/state/session
/mnt/SDCARD/flipos-dev/state/recent
/mnt/SDCARD/Roms
```

Canonical files remain exactly:

```text
state/settings/global
state/session/current
state/session/registry
state/recent/history
```

No on-disk migration is performed.

## Compile-time overrides

Existing host-test override behavior remains supported.

The following roots remain overrideable with compiler `-D` values:

- `FLIP_STORAGE_ROOT`
- `FLIP_ROMS_ROOT`
- `FLIP_SAVE_ROOT`
- `FLIP_STATE_ROOT`
- `FLIP_SESSION_RECOVERY_ROOT`
- `FLIP_SESSION_REGISTRY_ROOT`

Additional service-specific roots are also overrideable.

This preserves the existing Save Manager, State Manager, recovery and registry test isolation.

## Migrated consumers

Phase 7A routes these existing modules through the canonical path contract:

- Settings persistence
- Library/cache persistence
- Session current metadata
- GameSwitcher recent history
- Session registry
- Save Manager
- State Manager
- Save Safety recovery metadata
- frontend recent-history loading

The service behavior and persistence formats are unchanged.

## Paths service API

`src/runtime/flip_paths.c` exposes read-only accessors for the canonical roots.

This provides a stable service boundary for future runtime consumers without forcing every caller to know compile-time storage macros.

## Architecture guard

`tests/test_paths_architecture.py` fails if the migrated persistence modules reintroduce raw `/mnt/SDCARD/flipos-dev` literals.

The raw default layout is intentionally owned only by `include/flip_paths.h`.

## Safety

This milestone does not alter:

- save hashes or save filenames
- state hashes or state filenames
- Save Safety generations
- session registry format
- current-session format
- recent-history format
- library cache format
- ROM paths
- emulator launch protocol
- lifecycle behavior
- lid/deep suspend
- audio/RK817 handling
- NAND/MTD

## Host acceptance

Required:

- `make test-paths`
- `make test-session-registry`
- `make test-state`
- `make test-save`
- `make test-recovery`
- `make test-architecture`
- `make clean && make verify`

## Hardware acceptance

Because this is a path-ownership refactor, hardware acceptance should prove existing data is found at the exact same locations:

1. frontend boots normally
2. existing settings load
3. library/cache loads normally
4. existing GameSwitcher sessions remain visible
5. existing Red/Blue saves and states remain usable
6. launch/resume works without creating duplicate storage trees
7. MENU suspend/GameSwitcher works
8. lid automatic resume regression passes
9. no save/state/session files move or disappear

Only after those checks pass should the Phase 7 Paths/storage roadmap item be marked complete.


## Host validation result

Host validation passed:

- `make test-paths`
- `make test-session-registry`
- `make test-state`
- `make test-save`
- `make test-recovery`
- `make test-architecture`
- `make clean && make verify`

Built binaries:

```text
flip-frontend  8f3ae351c707e3f5f0aa9e2447eaff717ceb2a073cb6cddf068e9d9fcd0c785c
flip-gb        52aedcd285d88d185d269dde6abcbf92102288aa49641e22ea51af1b84c3dc1a
```

The `flip-gb` binary remained byte-for-byte identical to the Phase 6 hardware-tested binary.

## Hardware acceptance result

Hardware: Miyoo Flip RK3566, Linux 5.10.160, AArch64.

### Existing storage and metadata

The Phase 7A frontend booted successfully from the existing SD layout.

Observed:

```text
SETTINGS: loaded volume=5 brightness=200
FRONTEND_RECENT count=2
FRONTEND_RECENT index=0 ...Blue...
FRONTEND_RECENT index=1 ...Red...
```

Existing settings, library cache, current-session metadata, session registry, saves and states remained readable in their original locations.

No duplicate storage tree was created.

### Existing state resume

Pokémon Red resumed from the existing protected save/state paths:

```text
FLIP_SAVE: LOAD_CURRENT_OK size=32768
FLIP_STATE: RESTORE_CURRENT_OK path=/mnt/SDCARD/flipos-dev/states/gb/f06ea3b0fd4ad958.state size=59650
SESSION_STATE: state=running ...Red...
SESSION_REGISTRY: upsert state=running ... count=2
```

### GameSwitcher suspend

An injected physical-button-equivalent MENU event exercised the real input path.

Observed:

```text
FLIP_LIFECYCLE: gameswitcher requested
FLIP_SAVE: SAVE_OK path=/mnt/SDCARD/flipos-dev/saves/gb/f06ea3b0fd4ad958.sav size=32768
FLIP_STATE: SAVE_OK path=/mnt/SDCARD/flipos-dev/states/gb/f06ea3b0fd4ad958.state size=59650
SESSION: child exited code=20 action=2
SESSION_STATE: state=suspended ...Red...
SESSION_REGISTRY: upsert state=suspended ... count=2
FRONTEND_GAMESWITCHER requested
```

Blue remained independently recoverable in the session registry.

### Physical lid / deep-suspend regression

A real physical lid-close/open cycle produced:

```text
FLIP_LIFECYCLE: lid closed
FLIP_LIFECYCLE: suspend requested
FLIP_SAVE: SAVE_OK path=/mnt/SDCARD/flipos-dev/saves/gb/f06ea3b0fd4ad958.sav size=32768
FLIP_STATE: SAVE_OK path=/mnt/SDCARD/flipos-dev/states/gb/f06ea3b0fd4ad958.state size=59650
SESSION: child exited code=20 action=2
SESSION_STATE: state=suspended ...Red...
SESSION_REGISTRY: upsert state=suspended ... count=2
FRONTEND_STANDBY lid closed after suspend
POWER: deep sleep enter brightness=200
POWER: suspending
POWER: resumed
POWER: lid open
POWER: deep sleep exit brightness=200
FRONTEND_STANDBY auto resume ...Red...
FLIP_SAVE_SAFETY: AUTOMATIC_RESUME
flip_audio: RK817 state 17=06 38=00 40=69
flip_audio: RK817 deep-resume recovery required
flip_audio: RK817 deep-resume recovery applied
FLIP_STATE: RESTORE_CURRENT_OK path=/mnt/SDCARD/flipos-dev/states/gb/f06ea3b0fd4ad958.state size=59650
SESSION_STATE: state=running ...Red...
SESSION_REGISTRY: upsert state=running ... count=2
```

After wake, Red had a new process id/start timestamp and was `running`. Blue remained independently `interrupted`.

## Result

Phase 7A is hardware accepted.

The Paths/Storage service now owns canonical FlipOS storage locations without changing any existing on-disk path, format, identity or persistence behavior.
