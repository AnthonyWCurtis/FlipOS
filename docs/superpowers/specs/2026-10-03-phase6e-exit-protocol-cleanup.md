# FlipOS Phase 6E — Global Exit Protocol Cleanup

Date: 2026-10-03
Status: hardware accepted
Branch: `phase6/exit-protocol-cleanup`

## Goal

Finish Phase 6 by making the emulator-process exit protocol a global FlipOS contract instead of a GB/GBC session-adapter implementation detail.

This milestone must preserve all hardware-tested suspend, GameSwitcher, save/state, lid-resume and multi-session behavior.

## Previous behavior

The Lifecycle Host emitted:

- exit `0` for normal exit
- exit `20` for saved suspend/GameSwitcher

The GB/GBC session adapter independently knew that magic value `20` and translated it to `FLIP_SESSION_ACTION_SUSPEND`.

Any other normal process exit code was treated as a clean session exit. That meant an emulator process exiting with a failure such as `1` or `127` could be recorded as `exited` and removed from the session registry.

## Global protocol

Phase 6E introduces `flip_exit_protocol` as a system-independent process ABI.

Defined process exit codes:

```text
0  = normal exit
20 = saved suspend/GameSwitcher
```

Classification:

```text
0          -> NORMAL
20         -> SUSPEND
any other  -> ERROR
```

The Lifecycle Host and Session Manager share this protocol directly.

Session adapters no longer contain or expose an exit-code mapping callback.

## Session-state behavior

After a child process exits normally through `waitpid`:

- protocol NORMAL -> session action `EXIT` -> lifecycle state `exited`
- protocol SUSPEND -> session action `SUSPEND` -> lifecycle state `suspended`
- protocol ERROR -> session action `ERROR` -> lifecycle state `error`

Signal termination remains `signaled`.

Both `error` and `signaled` remain recoverable GameSwitcher states under the hardware-tested Phase 6D policy.

## Safety properties

This change does not alter:

- save or savestate formats
- Save Safety
- state restoration
- SRAM handling
- GameSwitcher identity
- registry identity
- lid/deep-suspend policy
- RK817 recovery
- automatic-resume intent
- NAND/MTD

Unexpected emulator failures now retain session metadata instead of being mistaken for clean exits.

## Host acceptance

Required:

- `make test-exit-protocol`
- `make test-lifecycle`
- `make test-session-save-safety`
- `make test-architecture`
- `make clean && make verify`

## Hardware acceptance

Required:

1. normal game launch and START+SELECT exit still records a clean exit
2. MENU/GameSwitcher still returns through protocol exit `20` and records `suspended`
3. exact suspended session resumes correctly
4. lid close/open still performs saved suspend and one-shot automatic resume
5. RK817 exact broken-signature recovery remains unchanged when encountered
6. multi-session registry state remains correct

A synthetic unexpected-exit classification is covered by host unit tests; hardware acceptance does not deliberately crash the emulator.


## Hardware acceptance result

Device: Miyoo Flip RK3566, Linux 5.10.160, AArch64.

### Clean exit

Pokémon Red was launched and exited with START+SELECT.

Observed:

```text
FLIP_LIFECYCLE: exit requested
FLIP_LIFECYCLE: exit save ok
SESSION: child exited code=0 action=1
SESSION_STATE: state=exited rom=...Pokemon - Red Version...
SESSION_REGISTRY: removed system=GB rom=...Pokemon - Red Version... count=1
```

The current record contained:

```text
state=exited
exit_code=0
signal=0
```

The independently suspended Pokémon Blue session remained in the registry.

### GameSwitcher suspend

Pokémon Red was launched again and MENU requested GameSwitcher.

Observed:

```text
FLIP_LIFECYCLE: gameswitcher requested
FLIP_LIFECYCLE: gameswitcher save ok
SESSION: child exited code=20 action=2
SESSION_STATE: state=suspended rom=...Pokemon - Red Version...
SESSION_REGISTRY: upsert state=suspended ... count=2
FRONTEND_GAMESWITCHER requested
```

The current record contained:

```text
state=suspended
exit_code=20
signal=0
```

Both Red and Blue remained independently suspended.

### Exact resume

Red was resumed from GameSwitcher.

Observed:

```text
SESSION_STATE: state=running rom=...Pokemon - Red Version...
SESSION_REGISTRY: upsert state=running ... count=2
FLIP_STATE: RESTORE_CURRENT_OK path=/mnt/SDCARD/flipos-dev/states/gb/f06ea3b0fd4ad958.state size=59650
```

Result:

```text
Red  -> running
Blue -> suspended
```

### Lid / automatic-resume regression

A fresh-marker lid cycle while Red was running produced:

```text
SESSION: child exited code=20 action=2
SESSION_STATE: state=suspended rom=...Pokemon - Red Version...
SESSION_REGISTRY: upsert state=suspended ... count=2
FRONTEND_STANDBY lid closed after suspend
POWER: deep sleep enter brightness=200
POWER: suspending
POWER: resumed
POWER: lid open
POWER: deep sleep exit brightness=200
FRONTEND_STANDBY auto resume rom=...Pokemon - Red Version...
FLIP_SAVE_SAFETY: AUTOMATIC_RESUME
flip_audio: RK817 state 17=06 38=00 40=69
flip_audio: RK817 deep-resume recovery required
flip_audio: RK817 deep-resume recovery applied
flip_audio: RK817 audio recovered
FLIP_STATE: RESTORE_CURRENT_OK path=/mnt/SDCARD/flipos-dev/states/gb/f06ea3b0fd4ad958.state size=59650
SESSION_STATE: state=running rom=...Pokemon - Red Version...
SESSION_REGISTRY: upsert state=running ... count=2
```

After wake, Red had a new process id/start timestamp and was `running`. Blue remained independently `suspended`.

## Result

Phase 6E is hardware accepted.

The emulator-process protocol is now a global FlipOS contract:

```text
0          -> clean exit
20         -> saved suspend/GameSwitcher
other code -> recoverable error
signal     -> recoverable signaled session
```

This completes the Phase 6 Global GameSwitcher / Session Manager roadmap.
