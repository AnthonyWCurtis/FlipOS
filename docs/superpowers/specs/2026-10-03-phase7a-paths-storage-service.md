# FlipOS Phase 7A — Paths / Storage Service

Date: 2026-10-03
Status: host validation pending
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
