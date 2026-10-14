# FlipOS Beta 6.1 — GPU Launch & Update Discovery Hotfix

Beta 6.1 is a focused hotfix for Beta 6.

## Fixed

- Moves the Mali GPU cold-start initialization into a guarded boot warm-up so the first game launch no longer has to pay the long black-screen startup delay.
- Keeps the normal guarded GPU-service startup path as a fallback if boot warm-up fails or times out.
- Adds startup timing diagnostics for the shared GPU service.
- Adds a lightweight background update check once FlipOS is running and Wi-Fi has a usable network address.
- Background update discovery fetches release metadata only; it does not silently download or install update packages.
- Shows the top-left download arrow as soon as a newer FlipOS release is found online.
- Clears both downloaded-package and online-update indicators after a successful update install.

## Hardware validation

On the Miyoo Flip test unit, the first Golden Sun: The Lost Age launch after a cold boot reached visible gameplay in about one second. Quick Menu operation and Start+Select return to GameSwitcher were also verified after the change.

## Updating

Existing FlipOS users can install this release through **Settings → System → System Update** once their current updater supports GitHub updates.

Fresh-install and update archives are provided below. Existing ROMs, BIOS files, saves, settings, and optimizer calibration data are not intentionally replaced by the update package.
