# FlipOS Phase 6B — Registry-backed Suspended Session Discovery

Date: 2026-10-03
Status: hardware accepted
Branch: `phase6/session-resume`

## Goal

Move GameSwitcher suspended-session discovery onto the Phase 6 persistent session registry without changing the hardware-tested Phase 5C resume, Save Safety, lid, save/state, or audio-recovery paths.

## Implementation

Phase 6B adds:

- `flip_session_registry_find_state()`
- newest-first lookup of a registry entry by lifecycle state
- GameSwitcher suspended-session discovery through the registry
- architecture coverage that prevents `load_suspended_session()` from reverting to `flip_session_read_current()`

The legacy `state/session/current` file is still written and retained for existing lifecycle behavior. Only suspended-session discovery moved to the registry in this milestone.

## Host acceptance

Passed on the development WSL2 environment:

- `make test-session-registry`
- `make test-architecture`
- `make clean && make verify`

The resulting Phase 6B frontend SHA-256 was:

```text
9e6bcc545c6432a2417dd9a0a13a86fc8e054fdf3ada092524f9acdb01d0a7b2
```

The `flip-gb` binary was unchanged from Phase 6A:

```text
52aedcd285d88d185d269dde6abcbf92102288aa49641e22ea51af1b84c3dc1a
```

## Hardware acceptance

Device: Miyoo Flip RK3566, Linux 5.10.160, AArch64.

ROM:

```text
/mnt/SDCARD/Roms/GB/Pokemon - Blue Version (USA, Europe).gb
```

### GameSwitcher suspend discovery

After pressing MENU while the game was running:

```text
current:  state=suspended
registry: state=suspended
```

The frontend logged:

```text
FRONTEND_SESSION suspended registry rom=/mnt/SDCARD/Roms/GB/Pokemon - Blue Version (USA, Europe).gb
```

This is the direct hardware proof that GameSwitcher discovered the suspended session through the registry.

### Deliberate resume

Resuming Pokémon Blue returned both records to:

```text
state=running
```

The registry remained at exactly one entry:

```text
SESSION_REGISTRY: upsert state=running ... count=1
```

No duplicate session was created.

### Lid regression

With Pokémon Blue running:

- lid close produced `state=suspended`
- frontend entered the existing standby path
- lid open logged `FRONTEND_STANDBY auto resume`
- Save Safety logged `FLIP_SAVE_SAFETY: AUTOMATIC_RESUME`
- exact RK817 broken signature `17=06 38=00 40=69` was observed
- deep-resume recovery was required and applied
- audio recovery completed successfully
- `RESTORE_CURRENT_OK` restored the emulator state
- registry returned to `state=running`
- the registry still contained one entry

Observed alternate working RK817 state `17=40 38=10 40=a5` was not treated as the exact broken signature.

## Result

Phase 6B is hardware accepted.

Registry-backed suspended-session discovery is now proven on the Miyoo Flip while the Phase 5C deliberate-resume and lid automatic-resume paths remain intact.
