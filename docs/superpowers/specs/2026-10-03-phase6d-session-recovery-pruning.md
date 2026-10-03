# FlipOS Phase 6D — Session Recovery and Registry Pruning

Date: 2026-10-03
Status: host validation pending
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
