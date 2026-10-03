# FlipOS Phase 6D — Session Recovery and Registry Pruning

Date: 2026-10-03
Status: hardware accepted
Branch: `phase6/session-recovery-pruning`

## Goal

Make the Phase 6 session registry self-healing at frontend startup without weakening FlipOS save/state safety.

Phase 6D addresses two remaining Session Manager requirements:

- prune stale session metadata
- recover interrupted sessions

This milestone changes only session metadata and GameSwitcher presentation. Emulator cartridge saves and savestates are never deleted or rewritten by reconciliation.

## Startup reconciliation

After the existing legacy-current recovery runs, FlipOS reconciles the persistent multi-session registry.

For each registry entry:

### Keep

The entry is preserved when:

- its system id still resolves
- its ROM still exists and is supported by that system
- its lifecycle state is one of the known recoverable/active states

Preserved recoverable states:

- `suspended`
- `interrupted`
- `signaled`
- `error`

### Recover

A registry entry still marked `running` is converted to `interrupted` when it is no longer the matching valid legacy current running session.

This handles abandoned registry metadata after crashes, reboots, or interrupted frontend/emulator lifecycles.

The original session identity and start timestamp are retained. Only lifecycle state and updated timestamp change.

### Prune

A registry entry is removed when:

- its system no longer exists
- its ROM no longer exists
- the ROM is no longer supported by the recorded system
- its lifecycle state is unknown
- a terminal `exited` or `closed` state incorrectly remains in the registry

Pruning removes only the registry entry.

It does **not** delete:

- `.sav`
- `.sav.prev`
- `.state`
- `.state.prev`
- recovery generations

## GameSwitcher recovery presentation

Registry-backed rows now distinguish recoverable sessions.

- `suspended` -> **SUSPENDED**
- `interrupted`, `signaled`, or `error` -> **RECOVER**
- ordinary historical entries with a resumable state -> **RESUME**

Recoverable rows expose:

```text
A RESUME   X CLOSE   B BACK
```

Closing an interrupted/error session removes its registry lifecycle entry and recent-history entry while preserving emulator save/state data.

## Existing behavior retained

- legacy `state/session/current` remains in place
- existing current-session recovery runs before registry reconciliation
- deliberate GameSwitcher resume continues through Save Safety
- lid automatic resume remains unchanged
- RK817 recovery remains unchanged
- emulator exit-code protocol is unchanged
- no NAND/MTD access

## Host acceptance

Required:

- `make test-session-registry`
- `make test-architecture`
- `make clean && make verify`

## Hardware acceptance

Planned controlled tests:

1. normal two-session GameSwitcher regression
2. create a synthetic stale registry entry for a missing ROM and reboot frontend; verify only metadata is pruned
3. create an orphaned `running` registry entry for a valid ROM; restart frontend and verify it becomes `interrupted`
4. verify GameSwitcher labels the entry **RECOVER**
5. resume that exact game and verify it returns to `running`
6. lid automatic-resume regression
7. verify save/state files remain present throughout


## Hardware acceptance result

Device: Miyoo Flip RK3566, Linux 5.10.160, AArch64.

### Stale registry pruning

A synthetically injected valid-format registry entry referencing:

```text
/mnt/SDCARD/Roms/GB/PHASE6D-MISSING-ROM.gb
```

was removed at frontend startup while the legitimate suspended Pokémon Blue session remained intact.

Observed:

```text
SESSION_REGISTRY: pruned state=suspended system=GB rom=/mnt/SDCARD/Roms/GB/PHASE6D-MISSING-ROM.gb
FRONTEND: session registry reconciled changes=1
```

Save/state file hashes remained unchanged across the reconciliation checkpoint.

### Interrupted session recovery

A synthetic Pokémon Red registry entry was injected as `state=running` while no matching current running session existed.

On frontend startup it became:

```text
state=interrupted
```

Observed:

```text
SESSION_REGISTRY: recovered interrupted system=GB rom=/mnt/SDCARD/Roms/GB/Pokemon - Red Version (USA, Europe).gb
FRONTEND: session registry reconciled changes=1
```

GameSwitcher visibly displayed:

- Red: **RECOVER**
- Blue: **SUSPENDED**

Resuming Red restored its existing state successfully:

```text
FLIP_STATE: RESTORE_CURRENT_OK path=/mnt/SDCARD/flipos-dev/states/gb/f06ea3b0fd4ad958.state size=59650
```

Registry result:

```text
Red  -> running
Blue -> suspended
```

### Lid regression

A fresh-marker lid cycle on running Pokémon Red produced:

```text
SESSION_STATE: state=suspended ...Red...
FRONTEND_STANDBY lid closed after suspend
POWER: deep sleep enter brightness=200
POWER: suspending
POWER: resumed
POWER: lid open
POWER: deep sleep exit brightness=200
FRONTEND_STANDBY auto resume rom=...Red...
FLIP_SAVE_SAFETY: AUTOMATIC_RESUME
flip_audio: RK817 state 17=06 38=00 40=69
flip_audio: RK817 deep-resume recovery required
flip_audio: RK817 deep-resume recovery applied
flip_audio: RK817 audio recovered
FLIP_STATE: RESTORE_CURRENT_OK ...Red...
SESSION_STATE: state=running ...Red...
SESSION_REGISTRY: upsert state=running ...Red... count=2
```

After wake:

- Red returned to `running` with a new process id/start timestamp
- Blue remained independently `suspended`
- the registry still contained both entries
- the exact known-broken RK817 signature triggered recovery
- Save Safety automatic-resume behavior remained unchanged

## Result

Phase 6D is hardware accepted.

At this point the Phase 6 Session Manager has hardware-proven support for suspend/save, persistent multi-session recording, switching/resuming, targeted close/remove, stale metadata pruning, and interrupted-session recovery. The remaining roadmap item is exit-code protocol cleanup.
