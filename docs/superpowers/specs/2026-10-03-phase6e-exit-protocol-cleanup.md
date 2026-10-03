# FlipOS Phase 6E — Global Exit Protocol Cleanup

Date: 2026-10-03
Status: host validation pending
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
