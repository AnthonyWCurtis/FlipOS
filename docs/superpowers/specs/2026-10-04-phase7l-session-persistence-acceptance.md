# FlipOS Phase 7L Session Persistence Hardware Acceptance

Date: 2026-10-04
Device: Miyoo Flip, RK3566

## Candidate

Hardware-tested code commit: `7cf8241`

Deployed SHA-256:

```text
flip-frontend 5490f7bd170229620c8e161fd491e9c76878153e6c5a792de83db84f2b693c4b
flip-gb       f1b4430758028be0a0dc3cc20919c1b9501ea8f331d8bd84980b4997317a1c12
```

`flip-gb` is byte-identical to the accepted Phase 7K build, so only the frontend was replaced. The Phase 7K frontend was preserved on-device as `flip-frontend.phase7k-accepted-backup`.

## Host gate

The exact deployed tree passed `make verify`.

Focused persistence tests passed for:

- no-record handling;
- exact version-1 write/read round-trip;
- byte-compatible current-session format;
- malformed-record rejection and quarantine;
- invalid-write rejection without replacing a valid committed record;
- Session Manager architecture ownership.

## Legacy compatibility proof

Before deployment, the new persistence parser was compiled as an ARM64 hardware probe and pointed at the existing live current-session record.

It read the existing record successfully without migration or rewrite:

```text
read=1
state=interrupted
system=GB
rom=/mnt/SDCARD/Roms/GB/Pokemon - Red Version (USA, Europe).gb
program=/mnt/SDCARD/flipos-dev/bin/flip-gb
pid=1233
started=1791128242
ended=1791128926
exit_code=0
signal=0
```

The raw on-disk record remained the established version-1 format.

## Deployment regression

After deploying only the verified frontend and rebooting:

- frontend hash matched `5490f7bd...`;
- unchanged GB hash remained `f1b44307...`;
- the frontend process started normally;
- the current record remained exactly the same interrupted Pokemon Red session;
- fresh deployed logs showed:

```text
SESSION_READ: state=interrupted system=GB rom=/mnt/SDCARD/Roms/GB/Pokemon - Red Version (USA, Europe).gb pid=1233
```

This proves the deployed Session Manager is consuming the extracted persistence service while preserving existing lifecycle semantics.

## Architecture result

Current-session disk-format ownership now lives in `flip_session_persistence`:

- atomic staged write + fflush + fsync + rename;
- strict version-1 parsing/validation;
- invalid-record quarantine.

Session Manager continues to own lifecycle/process decisions, recent history, recovery policy and registry mirroring.

The separate multi-session registry remains unchanged.

## Result

Phase 7L Session persistence is hardware accepted.

This completes the Phase 7 System Services service-ownership pass without changing the user-facing GameSwitcher/session model.
