# Unattended Device Regression Playbook

This note records the working method established on 2026-10-10 for unattended Miyoo Flip regression testing so it does not need to be rediscovered later.

## Purpose

Use this playbook to automate real-device Beta regression on a connected Miyoo Flip without needing a person to press buttons or physically close/open the lid.

The safe unattended scope includes gameplay launch, GameSwitcher, save/state integrity, optimizer apply/restore, real kernel suspend/resume, simulated lid state, RTC wake, volume/brightness persistence, logs, and host regression tests.

Do not deliberately force a known undervolt hard-lock or perform risky install/update experiments while the device is unattended.

## Device and ADB

Windows ADB path:

`C:\Users\Anthony\Desktop\platform-tools\adb.exe`

The Android/ADB serial exposed by this firmware is not a stable hardware identity and can change across boots.

For optimizer device binding use the Device Tree serial:

`/sys/firmware/devicetree/base/serial-number`

Observed stable Device Tree ID for this test Flip on 2026-10-10:

`af8184678a823e27`

Do not use `/proc/cpuinfo` Serial as the primary optimizer identity on this firmware. The optimizer calibration/runtime tools were changed to prefer the Device Tree serial and keep cpuinfo only as a fallback.

## Development-only input hooks

These hooks are intentionally volatile under `/tmp`. If the files do not exist, normal production behaviour remains authoritative.

### Lid state override

`/tmp/flipos-dev-lid-override`

Accepted contents:

- `open`
- `closed`

`src/runtime/flip_input.c` checks this development override first. If it is absent or malformed, FlipOS reads the real hall sensor from:

`/sys/devices/platform/hall-mh248/hallvalue`

The real hardware mapping is:

- hallvalue=1 -> lid OPEN
- hallvalue=0 -> lid CLOSED

### Auto-open after RTC wake

`/tmp/flipos-dev-lid-auto-open-after-resume`

If this marker exists, `flip_power_sleep_until_lid_open()` changes the development lid override to `open` immediately after a real kernel resume, then removes the marker.

This allows the complete unattended path:

simulated close -> real suspend -> RTC wake -> simulated open -> normal resume

It also supports testing wake-while-still-closed by omitting the auto-open marker for an initial wake. FlipOS should log `wake while closed; resleep` and suspend again.

### Button injection

`/tmp/flipos-dev-button`

Write one button name to this file. `flip_input_poll()` consumes it and removes the file. Supported development names include:

`UP DOWN LEFT RIGHT A B X Y L1 R1 L2 R2 START SELECT MENU L3 R3 VOL+ VOL-`

This hook was added because a generic `/dev/uinput` virtual keyboard is not sufficient: FlipOS opens the physical input device directly. The in-process dev hook exercises the same controls/lifecycle logic as real buttons.

## RTC wake

The RK817 RTC can wake the suspended Flip.

Typical test setup:

```sh
echo 0 > /sys/class/rtc/rtc0/wakealarm
echo +7 > /sys/class/rtc/rtc0/wakealarm
```

Then create the auto-open marker and set the development lid override to `closed`.

## Known-good unattended lid flow

Before forcing a close during a game, first establish an OPEN state so `flip_controls` sees a real open->closed transition. A close written while the current logical state is already closed will not request suspend.

Recommended sequence:

```sh
echo open > /tmp/flipos-dev-lid-override
sleep 2
echo 0 > /sys/class/rtc/rtc0/wakealarm
echo +7 > /sys/class/rtc/rtc0/wakealarm
echo 1 > /tmp/flipos-dev-lid-auto-open-after-resume
echo closed > /tmp/flipos-dev-lid-override
sync
```

Expected lifecycle evidence while a game is running:

- `FLIP_LIFECYCLE: lid closed`
- `FLIP_LIFECYCLE: suspend requested`
- `FLIP_SAVE: SAVE_OK`
- `FLIP_STATE: SAVE_OK`
- emulator clean exit
- runtime profile restored
- `FRONTEND_STANDBY lid closed after suspend`
- `POWER: deep sleep enter`
- `POWER: suspending`
- `POWER: resumed`
- `POWER: lid open`
- `POWER: deep sleep exit`
- `FRONTEND_STANDBY auto resume ...`
- runtime profile applied again
- automatic session resume
- state restored
- audio reopened/recovered

## GameSwitcher automation

Writing `MENU` to `/tmp/flipos-dev-button` while a game is running should exercise the normal GameSwitcher lifecycle:

- `FLIP_LIFECYCLE: gameswitcher requested`
- save RAM written
- savestate written
- emulator exits cleanly with suspend/GameSwitcher action
- session registry becomes suspended
- runtime CPU profile is restored
- frontend enters recent/GameSwitcher view

The GameSwitcher recent list normally selects the just-suspended title first. DOWN can be used to move to other recent games, then A to select.

Important: selecting an existing suspended game can intentionally open the Game Options/Resume menu. In automation, an additional A may be required to choose Resume. Do not classify that pause as a launch failure.

## Optimizer runtime checks

Use:

`/mnt/SDCARD/flipos-dev/bin/flip-optimizer-runtime status`

During gameplay the expected state is:

- `runtime_profile_active=1`
- `runtime_profile_marker=1`
- `runtime_profile_quarantined=0`

After returning fully to the frontend the expected state is:

- `runtime_profile_active=0`
- `runtime_profile_marker=0`
- `runtime_profile_quarantined=0`

Validated operating profile during this regression:

- 1.416 GHz -> 850000 uV
- 1.608 GHz -> 850000 uV
- 1.800 GHz -> 887500 uV
- 1.992 GHz -> 900000 uV

The 60-minute validation result remained passed. Normal regression must not mutate these voltages.

## Save/state integrity method

Before regression, hash all save files:

```sh
find /mnt/SDCARD/flipos-dev/saves -type f -print0 2>/dev/null \
  | xargs -0 sha256sum 2>/dev/null | sort \
  > /mnt/SDCARD/flipos-dev/logs/regression-saves-before.sha256
```

After regression:

```sh
find /mnt/SDCARD/flipos-dev/saves -type f -print0 2>/dev/null \
  | xargs -0 sha256sum 2>/dev/null | sort \
  > /mnt/SDCARD/flipos-dev/logs/regression-saves-after.sha256

diff -u \
  /mnt/SDCARD/flipos-dev/logs/regression-saves-before.sha256 \
  /mnt/SDCARD/flipos-dev/logs/regression-saves-after.sha256
```

Also check for zero-length saves/states:

```sh
find /mnt/SDCARD/flipos-dev/saves /mnt/SDCARD/flipos-dev/states \
  -type f -size 0 -print
```

2026-10-10 result: no save hash differences and no zero-byte save/state files after GB/GBC/GBA GameSwitcher and lid-suspend testing.

## Systems exercised successfully on 2026-10-10

- GB: Pokemon Blue
- GBC: Pokemon Crystal
- GBA: Pokemon LeafGreen
- GBA: Final Fantasy VI Advance

Observed successful paths included:

- normal game launch
- optimized profile apply
- GameSwitcher suspend/save
- switch between GB and GBA sessions
- GBC launch
- GB/GBC lid close -> save -> deep suspend -> RTC wake -> auto resume
- GBA session resume
- settings reload after resume
- RK817 deep-resume audio recovery

## Settings checks

Known persisted global settings file:

`/mnt/SDCARD/flipos-dev/state/settings/global`

During the 2026-10-10 regression the final values remained:

- volume=25
- brightness=255

VOL- followed by VOL+ returned to 25 and did not reproduce the old 0% -> 100% restore bug in this pass.

## Build/deploy regression

The playtest deploy script now pushes rebuilt common binaries including:

- `build/flip-frontend`
- `build/flip-gb`
- `build/flip-gba`

This matters because lid/power/input code is shared by the frontend and emulator runners.

After any fix, run:

```sh
make verify
git diff --check
```

2026-10-10 final result: both passed.

## Useful logs/state

Frontend/device log:

`/mnt/SDCARD/flipos-dev/logs/frontend.log`

Playtest telemetry:

`/mnt/SDCARD/flipos-dev/logs/playtest/`

Session state:

`/mnt/SDCARD/flipos-dev/state/session/current`

Session registry:

`/mnt/SDCARD/flipos-dev/state/session/registry`

Optimizer state:

`/mnt/SDCARD/flipos-dev/state/optimizer/`

Settings:

`/mnt/SDCARD/flipos-dev/state/settings/global`

## Regression status as of 2026-10-10

Consider the gameplay + GameSwitcher + suspend/resume + save-safety regression gate largely passed.

Next roadmap focus after this pass: core UX polish, especially volume/mute behaviour, brightness, user-facing wording/status screens, and any remaining developer-facing messages.