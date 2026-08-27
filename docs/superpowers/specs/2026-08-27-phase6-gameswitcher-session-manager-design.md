# FlipOS Phase 6 Global GameSwitcher / Session Manager Design

## Goal

Build a system-agnostic GameSwitcher / Session Manager that can suspend, record, switch, resume, close, prune, and recover game sessions without depending on Game Boy-specific behavior.

The design must preserve the already-proven Phase 5 save/state safety rules and remain suitable for later systems such as GBA/mGBA.

## Architectural rule

> GameSwitcher owns which sessions exist and which session the player wants. State Manager owns emulator snapshots. Save Manager owns cartridge progress. Session Adapter knows how to launch each system.

No GameSwitcher code may parse Game Boy save data, construct GB-specific save/state paths, or depend on Gambatte behavior.

## Scope

Phase 6 includes:

- suspend the current game through the existing lifecycle save path;
- record successful suspended sessions persistently;
- maintain multiple suspended sessions until the user explicitly closes them;
- switch between suspended sessions;
- resume a selected previous game through the normal system/session adapter path;
- close/remove GameSwitcher sessions without deleting saves or states;
- prune stale or invalid session entries;
- recover interrupted session metadata after crash/reboot;
- centralize emulator lifecycle handoff policy;
- clean up the existing exit-code protocol without breaking the proven exit-code-20 behavior during migration;
- provide a generic GameSwitcher UI and library handoff.

Phase 6 does not include:

- new emulator cores;
- deleting `.sav`, `.state`, recovery files, or other game progress when a session is closed;
- arbitrary session-count limits;
- automatic eviction of valid sessions;
- NAND/MTD writes;
- multi-generation save backup policy.

## User model

GameSwitcher sessions are durable until the player explicitly closes them.

There are three distinct concepts:

- **Library**: all installed games.
- **Recent**: games recently played.
- **GameSwitcher Sessions**: games deliberately suspended and retained for fast resume.

These concepts must not be conflated.

A minimal GameSwitcher view is conceptually:

```text
GAME SWITCHER

Pokemon Yellow
Super Mario Land
Tetris

+ NEW GAME
```

Selecting a retained session offers:

```text
RESUME
CLOSE SESSION
```

`NEW GAME` returns to the normal library flow.

## Session identity

Each retained session is identified generically by:

- system id;
- exact ROM path;
- the existing frozen ROM identity/hash where a stable compact key is required.

The Session Manager does not own emulator state bytes. It records metadata that points to a logical game session, while resume availability is determined through the registered system/session adapter and existing recovery policy.

## Persistent session registry

Add a durable multi-session registry under SD-card FlipOS storage, conceptually:

```text
/mnt/SDCARD/flipos-dev/sessions/registry
```

The final exact path may reuse the existing FlipOS session storage root, but it must remain SD-card-only.

Each entry contains enough information to reconstruct and validate a session, including at minimum:

```text
system=<system-id>
rom=<exact-rom-path>
status=<suspended|interrupted>
last_active=<epoch>
```

Implementation may use one atomic registry file or one file per session, provided the public Session Manager interface is independent of the storage layout.

Registry writes use staged temporary files, file-data flush/fsync, and rename before replacement. On FAT32/vfat this improves transaction safety but does not claim journaled durability.

## Ownership boundaries

### Session Manager

Owns:

- the durable set of GameSwitcher sessions;
- session identity and metadata;
- add/update after successful suspension;
- remove/close semantics;
- stale-session pruning;
- interrupted-session repair/reclassification;
- selecting/ordering retained sessions;
- interpretation of lifecycle handoff results into GameSwitcher policy.

It does not serialize emulator state or cartridge SRAM.

### State Manager

Continues to own:

- `.state` identity and storage;
- structural validation;
- atomic state saves;
- state recovery/fallback operations.

### Save Manager

Continues to own:

- `.sav` identity and storage;
- protected current/previous cartridge progress;
- recovery save generations.

### Recovery policy

Phase 5C remains authoritative for timeline safety.

A GameSwitcher session must never bypass `.restore-pending`. If resume is blocked, GameSwitcher must not present the session as directly resumable even when a state file physically exists.

### Session Adapter

Continues to own system-specific launch behavior and resume-availability checks.

Adding GBA later should require a GBA adapter implementation, not GameSwitcher changes.

### Frontend

Owns presentation and player navigation only:

- show retained sessions;
- request resume;
- request close;
- return to library for a new game.

The frontend must not parse registry files directly or know save/state storage rules.

## Lifecycle flow

### Suspend into GameSwitcher

```text
Playing
  -> GameSwitcher lifecycle request
  -> cartridge save commit
  -> emulator state commit
  -> lifecycle reports successful suspend handoff
  -> Session Manager records/updates retained session
  -> frontend shows GameSwitcher
```

A session is recorded only after the controlled persistence path succeeds.

If cartridge save or state save fails, existing lifecycle safety behavior remains authoritative: the emulator does not claim a successful suspend handoff, and Session Manager must not create a new resumable session entry from that failed transition.

### Resume retained session

```text
GameSwitcher
  -> select session
  -> Session Manager validates metadata
  -> Session Adapter checks resume policy/availability
  -> normal system launcher runs
  -> Save Manager loads cartridge progress
  -> State Manager restores resumable state when permitted
  -> Playing
```

Resuming a session does not remove it from the registry. The session remains retained until explicitly closed. Its `last_active` metadata may be updated.

### Switch games

Switching from game A to game B means:

1. A completes the normal controlled GameSwitcher suspend path.
2. A is present/updated in the registry.
3. GameSwitcher selects B.
4. B launches through its system adapter.
5. B restores using its own valid save/state/recovery policy.

No core-specific cross-game logic exists in Session Manager.

## Close Session semantics

Closing a GameSwitcher session removes only Session Manager metadata for that game.

It must not delete:

- `.sav`;
- `.sav.prev`;
- `.sav.recovery`;
- `.state`;
- `.state.prev`;
- `.state.recovery`;
- restore markers or recovery data owned by Phase 5C.

Therefore closing a session means "remove from GameSwitcher", not "delete game progress".

## Pruning policy

Automatic pruning is conservative and limited to unusable metadata.

A retained session may be pruned when:

- its registry record is malformed;
- its system id is no longer registered/supported;
- its ROM no longer exists or is not readable;
- its ROM path is no longer supported by the registered system;
- it is a duplicate of another canonical session entry.

A valid session is never automatically evicted simply because many sessions exist or because it is old.

Missing resume state alone should not cause destructive deletion of save/state data. The UI may mark the session non-resumable or require normal launch according to adapter/recovery policy.

## Interrupted-session recovery

The existing single current-session metadata and stale-running recovery behavior become input to the new Session Manager rather than a competing source of truth.

On frontend/startup recovery:

- detect a persisted session marked running whose process no longer exists or no longer matches;
- classify it as interrupted rather than running;
- preserve any already-committed save/state data;
- reconcile it with the retained-session registry when enough metadata is valid;
- never fabricate resume availability without asking the Session Adapter/recovery policy;
- never delete cartridge progress as part of interrupted-session repair.

This allows FlipOS to recover from crashes or power loss without confusing stale process metadata with a live emulator.

## Exit-code protocol cleanup

Existing compatibility behavior is initially preserved:

```text
0  = normal controlled exit
20 = successful suspend/GameSwitcher handoff
```

Phase 6 centralizes interpretation of raw emulator exit codes in the Session Adapter/Session Manager boundary.

Higher layers should consume semantic actions such as:

- normal exit;
- GameSwitcher suspend handoff;
- error/interrupted exit.

The goal is to prevent raw numeric `20` handling from spreading further through the frontend and future emulator integrations while avoiding a risky protocol break during this phase.

## Ordering

GameSwitcher shows retained sessions in most-recently-active order.

Resuming or successfully suspending a session updates `last_active` and moves it to the top.

There is no automatic valid-session count limit in Phase 6.

## Error handling

Safety priorities:

1. Never report a failed persistence transition as a resumable suspended session.
2. Never bypass Phase 5C resume blocking or timeline protection.
3. Never delete save/state data when closing or pruning Session Manager metadata.
4. Never treat malformed registry data as authoritative.
5. Preserve valid sessions when one registry entry is damaged whenever the storage representation permits recovery.
6. Prefer returning the player to the library or a non-resumable session state over launching ambiguous data.

## Testing strategy

Phase 6 is developed test-first.

### Session Manager unit tests

Cover:

- empty registry;
- add suspended session;
- update existing session without duplication;
- multiple retained sessions;
- most-recent ordering;
- explicit close/remove;
- malformed entry handling;
- missing/unsupported ROM pruning;
- duplicate pruning;
- interrupted-session repair;
- atomic registry replacement failure behavior.

### Architecture tests

Require:

- no Game Boy/Gambatte-specific behavior in Session Manager/GameSwitcher;
- frontend does not parse registry files directly;
- Session Manager does not manipulate `.sav` or `.state` bytes;
- resume availability flows through Session Adapter/recovery policy;
- raw exit-code mapping remains centralized.

### Lifecycle integration tests

Require:

- successful GameSwitcher persistence records/updates a session;
- failed cartridge save creates no successful handoff session;
- failed state save creates no successful handoff session;
- normal exit does not incorrectly create a suspended session;
- exit-code-20 compatibility maps to semantic GameSwitcher handoff at one boundary only.

### Frontend tests

Require:

- list retained sessions;
- resume selection delegates through Session Manager/Session Adapter;
- close removes metadata only;
- New Game returns to library;
- non-resumable/recovery-blocked session is not falsely offered as Resume.

## Real-Miyoo-Flip hardware acceptance

Hardware acceptance should prove at least:

1. Start Pokemon Yellow and request GameSwitcher.
2. `.sav` and `.state` commit successfully before handoff.
3. Pokemon appears as a retained GameSwitcher session.
4. Launch a second game through the library and suspend it too.
5. Both sessions remain visible with no automatic eviction.
6. Resume Pokemon and confirm exact previous state/progress.
7. Switch back to the second game and confirm its state/progress.
8. Reboot/restart FlipOS and confirm both valid sessions remain registered.
9. Resume both after reboot through generic Session Adapter paths.
10. Close one session and prove it disappears from GameSwitcher while its save/state files remain untouched.
11. Simulate or detect stale/interrupted session metadata and prove recovery does not destroy save/state data.
12. Verify a Phase 5C restore-pending session is not falsely auto-resumed.
13. Verify START+SELECT normal exit behavior remains correct.
14. Verify lid close/open still performs direct automatic wake resume rather than opening GameSwitcher.
15. Verify RK817 exact-signature deep-resume recovery, 32768->32000 audio conversion, and established frame timing remain unchanged for GB.

## Milestones

### Phase 6A — Session Registry

- generic durable multi-session registry;
- add/update/read/remove;
- ordering;
- structural validation and pruning;
- unit tests.

### Phase 6B — Lifecycle Integration

- central semantic handoff result;
- record session only after successful controlled GameSwitcher persistence;
- preserve exit-code-20 compatibility at one boundary;
- integration tests.

### Phase 6C — GameSwitcher UI

- list retained sessions;
- resume selected session;
- New Game/library handoff;
- generic adapter-based launch behavior.

### Phase 6D — Close Session

- explicit close/remove action;
- metadata-only deletion;
- save/state preservation tests.

### Phase 6E — Restart / Interrupted Recovery

- reconcile stale current-session metadata;
- recover/reclassify interrupted sessions;
- prune invalid registry entries;
- reboot/startup behavior.

### Phase 6F — Full Software and Hardware Gate

- full native regression suite;
- ARM64 static build;
- compact verification target;
- exact candidate hashes;
- real-Miyoo-Flip multi-session switching and reboot acceptance;
- lifecycle/audio/timing/save-safety regression checks.

## Summary

Phase 6 turns the existing single-session/recent-history foundation into a generic durable GameSwitcher session subsystem.

The Session Manager owns session membership and lifecycle policy, while Save Manager, State Manager, Recovery policy, Session Adapter, and frontend retain their existing focused responsibilities. Valid sessions remain until the player closes them. Automatic pruning removes only invalid metadata, never game progress.

The result is an OnionOS-like persistent GameSwitcher model that is native to FlipOS and ready for future systems without embedding Game Boy-specific assumptions.