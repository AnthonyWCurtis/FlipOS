# FlipOS

FlipOS is an independent, lightweight console-style operating environment for the **Miyoo Flip (Rockchip RK3566)**. It runs from an SD card and prioritises native emulation, fast launch/resume, safe game saves, lid-aware power management, and a simple controller-first interface.

## Current release: Beta 6.1

**[Download FlipOS v0.1.0-beta.6.1](https://github.com/AnthonyWCurtis/FlipOS/releases/tag/v0.1.0-beta.6.1)** — *GPU Launch & Update Discovery Hotfix*.

FlipOS remains **prerelease testing software**. Back up your SD card and saves before installing or updating. Some recovery, battery-life, Wi-Fi and long-duration hardware scenarios still need broader testing.

### Currently supported systems

- Game Boy and Game Boy Color — Gambatte
- Game Boy Advance — mGBA

The initial releases focus on these **native** systems. Additional consoles and an optional RetroArch edition are deferred until the existing experience is reliable.

### Features

- Console-style Home, Library, GameSwitcher, Play Stats and in-game Quick Menu
- Per-game save data, resume states and guarded recovery paths
- Lid sleep/wake, battery display, software Power Menu and low-battery save protection
- Shared video, input, timing, audio and lifecycle services, including a GPU-backed presentation path
- Dynamic CPU/GPU performance management and guarded per-device CPU optimisation
- Wi-Fi setup, remembered networks and USB/MTP transfer of **ROMs**, **BIOS** and **Saves**
- Hardware RTC, time-zone and time-sync support
- First-boot setup and GitHub Release updates with package verification and rollback staging
- Beta 6.1 GPU startup warm-up and background *update discovery* (no automatic update installation)

The **experimental below-spec CPU undervolt research** is not included in the Beta 6 production feature set.

## Installation and updates

For a fresh installation, download `FlipOS-Fresh_Install.zip` from the release page and follow the [SD-card installation instructions](docs/BETA1_INSTALL.md). The original guide applies to the Beta installation layout; use the latest release assets rather than downloading an earlier Beta.

For an existing supported installation, use **Settings → System → System Update** or consult the release notes before installing manually. Do not remove power or the SD card during an update.

FlipOS does **not** distribute commercial ROMs or copyrighted BIOS files; provide your own legally obtained files.

## Documentation

- [Beta 6.1 release notes](release/BETA61_NOTES.md)
- [Beta 6 changes and hardware validation](release/BETA6_NOTES.md)
- [Canonical engineering roadmap](docs/ROADMAP.md)
- [Hardware regression playbook](docs/UNATTENDED_REGRESSION_PLAYBOOK.md)
- [Third-party notices](docs/THIRD_PARTY_NOTICES.md)
- [Historical Beta 1 known issues](docs/BETA1_KNOWN_ISSUES.md) (some items have since been addressed)

## Reporting problems

Please [open a GitHub Issue](https://github.com/AnthonyWCurtis/FlipOS/issues) and include the FlipOS version, game/system, steps to reproduce, whether the issue followed boot/lid sleep/GameSwitcher/update, and any available logs or photos.

## Project status

FlipOS is a community project and is **not affiliated with Miyoo**. It is not a replacement for the internal NAND firmware; the project follows an SD-card-first approach and preserves the stock recovery path.
