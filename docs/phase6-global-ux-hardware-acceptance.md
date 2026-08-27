# Phase 6 Global UX Hardware Acceptance

Hardware:
- Miyoo Flip
- RK3566
- Stock kernel environment
- SD-card FlipOS development runtime

Branch:
- phase6/gameswitcher-session-manager

## Hardware-confirmed behavior

### Global OSD

Confirmed:
- Volume controls function in GameSwitcher.
- Brightness controls function in GameSwitcher.
- Shared OSD presentation is used across FlipOS-owned frontend screens.
- Volume and brightness feedback is visible through the shared OSD path.

### Global controls

Confirmed:
- Global controls are available in GameSwitcher.
- Save Safety now uses the same shared global-controls architecture.
- MENU + SELECT remains the frontend power-off gesture.

### Exit Game

Confirmed on hardware:
- SELECT + START no longer exits the game.
- SELECT + START remains available as normal game input.
- MENU + START exits the running game.
- MENU release after the exit chord does not trigger GameSwitcher.

### Global screensaver

Confirmed on hardware:
- Screensaver activates after approximately 30 seconds of frontend inactivity.
- Screensaver displays HH:MM and FLIPOS.
- Screensaver uses a black background.
- Clock/FLIPOS block moves with slow DVD-style bouncing motion.
- Motion is throttled to approximately 1 Hz.
- Wake input is consumed and does not activate the selected frontend action.
- Screensaver architecture is shared across frontend-owned screens.

### Interrupted Session Recovery

Confirmed on hardware:
- A real running GB session was interrupted by rebooting while `flip-gb` was active.
- Before reboot, persisted current-session metadata reported `state=running`.
- On FlipOS startup, the stale running session was rewritten to `state=interrupted`.
- The generic retained-session registry created the matching GameSwitcher entry with `status=interrupted`.
- Recovery preserved emulator savestate data.
- No stale `flip-gb` process remained after reboot.
- Recovery log reported `SESSION_RECOVER: interrupted ... savestate=preserved`.
- Hardware test used Pokemon Yellow with retained session hash `792f9628ba43b7d9`.
- The recovered interrupted GameSwitcher entry resumed successfully.
- Resume restored the existing savestate with `FLIP_STATE: RESTORE_CURRENT_OK`.
- The resumed game returned to `state=running`.
- Pressing MENU after recovery saved and suspended normally.
- The retained session transitioned from `status=interrupted` back to `status=suspended`.
- GameSwitcher reopened normally after the post-recovery suspend.

### Stale Session Pruning

Confirmed on hardware:
- GameSwitcher invokes generic stale-session pruning when its model loads.
- Malformed `.session` metadata is removed from persistent session storage.
- Healthy retained sessions remain intact while malformed records are pruned.
- Wrong-hash/orphan session files are rejected during explicit pruning.
- FAT32 case-preserved system directory spelling (`gb` vs `GB`) must not be treated as a stale-session condition.
- Canonical validation therefore checks the hashed `.session` filename rather than requiring a case-sensitive full-path match.
- Hardware retest preserved healthy session `792f9628ba43b7d9.session` while removing the malformed `broken.session`.

### Reset Game Data

Confirmed on hardware:
- GBC Reset Game Data correctly resolves the public GBC system identity to the shared `gb` persistence namespace.
- Pokemon Crystal session, save, previous save, state, and previous state artifacts were moved into `/mnt/sdcard/flipos-dev/reset/`.
- No matching active Crystal session/save/state artifacts remained after reset.
- Runtime logged `FLIP_GAME_DATA: RESET_OK system=GBC`.
- Empty resets now fail with `RESET_NONE`; `RESET_OK` therefore means at least one artifact was actually quarantined.

### Clock limitation

Confirmed:
- Displayed clock is currently approximately one hour behind UK local time
  during BST.
- Current device time presentation appears to be UTC.
- No hard-coded offset should be added.
- Timezone/NTP/network clock handling remains future work.

## Deployed binary hashes

flip-frontend:
ce3231e1a4396e5c795d3745d353baa3c8c8f8ccc68ca00f034798df04909ba4

flip-gb:
44f3cc56be0a3cea2ce28002d736c38e3ebaa61668b65fda2abb468a4deaf929

## Verification

- make verify: PASS
- local/device flip-frontend hashes: MATCH
- local/device flip-gb hashes: MATCH
