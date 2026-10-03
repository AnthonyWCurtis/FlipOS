# FlipOS Phase 6C — Multi-session GameSwitcher Identity

Date: 2026-10-03
Status: hardware accepted
Branch: `phase6/multi-session-gameswitcher`

## Goal

Allow GameSwitcher to manage more than one suspended session independently by carrying exact `(system, ROM)` identity through row status, resume, close, and history removal.

## Implementation

Phase 6C changes GameSwitcher from one global suspended-ROM assumption to per-row session lookup.

Key changes:

- each GameSwitcher row resolves its own registry entry by `(system, ROM)`
- each row determines suspended state independently
- resume availability uses the row's own system
- confirmation state preserves both system id and ROM path
- CLOSE targets the exact suspended registry session
- REMOVE targets the exact recent-history entry
- closing a non-current suspended registry session removes only that registry lifecycle entry
- emulator savestates and SRAM are preserved
- if the target is also the legacy current suspended session, the existing Phase 5C close path is retained

## Host acceptance

Passed:

- `make test-session-registry`
- `make test-architecture`
- `make clean && make verify`

Phase 6C frontend SHA-256:

```text
6d24f744cc31c38d9a23dc41cb08ce8deafb44e663b67df1e007512264c3017e
```

`flip-gb` remained unchanged:

```text
52aedcd285d88d185d269dde6abcbf92102288aa49641e22ea51af1b84c3dc1a
```

## Hardware acceptance

Device: Miyoo Flip RK3566, Linux 5.10.160, AArch64.

ROMs:

```text
/mnt/SDCARD/Roms/GB/Pokemon - Blue Version (USA, Europe).gb
/mnt/SDCARD/Roms/GB/Pokemon - Red Version (USA, Europe).gb
```

### Two suspended sessions

Test flow:

1. launch Blue
2. MENU -> suspend Blue
3. return to Games
4. launch Red
5. MENU -> suspend Red

Registry result:

```text
Red  -> suspended
Blue -> suspended
```

Both entries coexisted in the registry and recent history.

### Resume one session without disturbing another

Blue was selected from GameSwitcher and resumed.

Registry result:

```text
Blue -> running
Red  -> suspended
```

The registry remained at two entries, proving the selected session was resumed by exact identity and the other suspended session was preserved.

### Close one suspended session without disturbing another

Blue was suspended again, then Red was selected and closed.

Registry result:

```text
Blue -> suspended
Red  -> removed
```

Recent history retained only Blue.

Relevant logs:

```text
FRONTEND_GAMESWITCHER action requested=close rom=/mnt/SDCARD/Roms/GB/Pokemon - Red Version (USA, Europe).gb
SESSION_REGISTRY: removed system=GB rom=/mnt/SDCARD/Roms/GB/Pokemon - Red Version (USA, Europe).gb count=1
SESSION_CLOSE: closed registered rom=/mnt/SDCARD/Roms/GB/Pokemon - Red Version (USA, Europe).gb savestate=preserved
FRONTEND_GAMESWITCHER closed=/mnt/SDCARD/Roms/GB/Pokemon - Red Version (USA, Europe).gb
FRONTEND_GAMESWITCHER removed=/mnt/SDCARD/Roms/GB/Pokemon - Red Version (USA, Europe).gb
```

This proves CLOSE operates on the selected suspended session rather than whichever session happens to be in the legacy `current` record.

## Result

Phase 6C is hardware accepted.

FlipOS can now persist, display, resume, and close multiple suspended sessions independently by `(system, ROM)` identity while preserving emulator save/state data.
