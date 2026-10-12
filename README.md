# FlipOS

FlipOS is a lightweight custom operating environment built specifically for the **Miyoo Flip (RK3566)**.

The project is focused on a simple console-like experience: fast game launch and resume, lid-aware sleep/wake, GameSwitcher, native emulator integration, sensible power management, Wi-Fi, USB file transfer, and minimal UI overhead.

## Beta 4

**FlipOS Beta 4 is testing software.** It is usable, but it is not considered finished or fully hardened yet.

If you find a bug, crash, odd resume behaviour, broken game, Wi-Fi problem, update problem, battery issue, or anything else that does not feel right, please report it in **GitHub Issues**:

https://github.com/AnthonyWCurtis/FlipOS/issues

When possible, include:
- what you were doing;
- the game/system involved;
- whether the problem happened after boot, resume, lid sleep, or GameSwitcher;
- whether it happens every time;
- photos/video if useful.

## Supported in Beta 4

Beta 4 currently focuses on:
- Game Boy;
- Game Boy Color;
- Game Boy Advance.

NES, SNES, Mega Drive, PS1, RetroArch compatibility, themes, richer metadata/artwork, and additional emulator expansion are not part of Beta 4.

## Highlights

- GB/GBC via Gambatte and GBA via mGBA, built from source.
- Fast suspend/resume and lid handling.
- GameSwitcher and per-game resume states.
- Readable save filenames matching ROM names.
- Automatic migration of older FlipOS hashed save filenames when a game is loaded.
- Hardware RTC, timezone, automatic time/NTP and manual Date & Time support.
- Wi-Fi scan/connect/password/reconnect support.
- Automatic USB MTP file access: **ROMs**, **BIOS**, and **Saves** appear on a connected PC as **FlipOS**.
- Dynamic CPU performance control and guarded per-device optimization.
- Autonomous per-device CPU voltage calibration with crash-safe recovery, live progress, and validation-gated tuned profiles.
- Smart charging policy and low-battery save-safe shutdown path.
- First-boot setup wizard.
- GitHub Release based update path with validation and rollback staging.

## Important

FlipOS does **not** include commercial ROMs or copyrighted BIOS files. You must provide your own legally obtained files.

Read the [installation guide](docs/BETA1_INSTALL.md), [Beta known issues](docs/BETA1_KNOWN_ISSUES.md), and [Beta 4 release notes](docs/BETA4_RELEASE_NOTES.md) before installing.

## Status

This is an independent community project and is not affiliated with Miyoo.
