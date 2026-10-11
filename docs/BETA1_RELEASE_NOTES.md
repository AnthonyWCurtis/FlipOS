# FlipOS v0.1.0-beta.1

**First public Beta.**

FlipOS is now ready for wider testing on the Miyoo Flip. This is still **Beta software**, so users should expect rough edges and are strongly encouraged to report anything that does not behave correctly.

Report issues: https://github.com/AnthonyWCurtis/FlipOS/issues

## What is included

- Native FlipOS frontend focused on the Miyoo Flip.
- Game Boy / Game Boy Color support using Gambatte.
- Game Boy Advance support using mGBA.
- GameSwitcher and resume-state workflow.
- Lid-aware sleep/wake and fast resume.
- Audio resume/recovery.
- Volume and brightness controls.
- Battery indicator and low-battery save-safe shutdown.
- Dynamic CPU performance control.
- Guarded per-device CPU optimisation.
- Smart charging support.
- Wi-Fi setup, reconnect and saved-network management.
- Hardware RTC, timezone, NTP and manual Date & Time.
- GB/GBC cartridge RTC persistence.
- First-boot setup wizard and on-screen keyboard.
- USB MTP: connect to a PC and access ROMs, BIOS and Saves through a device named **FlipOS**.
- Human-readable save filenames matching ROM names.
- Automatic migration from older FlipOS hashed save names.
- GitHub Release based update download, validation, staging and rollback support.

## Beta notes

- Save Safety restore/undo UI is deliberately hidden until it is more thoroughly hardened.
- The experimental shared GPU host is disabled; Beta 1 uses the stable direct CPU/DRM path.
- Long suspend/idle drain, broader RTC game coverage and update-interruption recovery need more public testing.
- Only GB, GBC and GBA are in scope for this release.

## Please report issues

This release exists to get FlipOS onto more real devices and uncover things a single development unit cannot.

If something feels wrong—even if it seems minor—please open a GitHub issue:
https://github.com/AnthonyWCurtis/FlipOS/issues

Useful reports include what you were doing, which game/system was running, whether the issue followed boot/resume/GameSwitcher, and whether it is reproducible.
