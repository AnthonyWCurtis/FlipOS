# FlipOS Beta Readiness

**Historical release gate:** the checklist below documents the original Beta 1 decision on 2026-10-11. It is preserved for auditability, **not** a complete Beta 6.1 sign-off.

**Current published prerelease:** [FlipOS v0.1.0-beta.6.1](https://github.com/AnthonyWCurtis/FlipOS/releases/tag/v0.1.0-beta.6.1), 14 October 2026. That public GitHub release confirms publication/asset availability; it does not automatically close the unchecked RTC, Wi-Fi, interrupted-update, deep-suspend, crash/recovery or full game-regression acceptance items. They remain in the P1 device-stability phase of [the canonical roadmap](ROADMAP.md).

Status date: 2026-10-11 (Beta 1 decision)

## Must finish before public beta

The statuses immediately below are a **historical Beta 1 readiness snapshot**. Later Beta 5/6.1 shipped additional features; do not use these boxes alone as the current stable-release gate.

- [x] Core GB/GBC/GBA gameplay stability
- [x] Save and resume-state integrity baseline
- [x] GameSwitcher stability baseline
- [x] Lid sleep/wake baseline
- [x] Audio resume/recovery
- [x] Volume/brightness controls and hold repeat
- [x] Frontend UX cleanup
- [x] Battery indicator
- [x] ROM display-title cleanup
- [x] Dynamic CPU performance controller
- [x] Per-device CPU optimizer with guarded recovery
- [x] Smart charging policy + RK817 runtime helper
- [x] Save-safe low-battery shutdown path
- [x] Wi-Fi scan/connect/password/reconnect backend
- [x] Wi-Fi first-boot integration
- [x] Automatic USB MTP access for ROMs, BIOS and save backups
- [x] Hardware RTC + timezone + NTP/manual Date & Time support
- [x] GB/GBC cartridge RTC persistence path
- [ ] Final real-device RTC game regression (GB/GBC + GBA)
- [x] First-boot recovery/completion markers
- [x] Reusable on-screen keyboard component
- [x] Keyboard visibly switches lowercase/uppercase
- [x] SD-card update package validation
- [x] Update staging and rollback copy
- [x] Update UI confirmation and failure wording
- [x] GitHub Release update download path
- [ ] Publicly accessible GitHub release endpoint + first Beta release asset
- [ ] Final real-device Wi-Fi visual/interaction approval
- [ ] Final updater interruption/rollback torture test
- [ ] Longer suspend soak and idle-drain characterization
- [ ] Full log/error sweep after feature freeze
- [ ] Reboot/recovery torture pass
- [ ] Final GB/GBC/GBA regression after feature freeze
- [x] Release-grade install/upgrade package
- [x] Version/build identity finalized for Beta 1: v0.1.0-beta.1
- [x] Licence/source/notices audit
- [x] Beta install instructions + known-issues document

## Nice to have before beta

- [x] Wi-Fi signal icons and secured-network lock icon
- [x] Wi-Fi limited/no-address indicator
- [x] Saved-network forget action
- [ ] Live updater percentage display while files are being installed
- [x] Wi-Fi indicator in the global top bar outside the Wi-Fi page
- [ ] Optimizer results wording for profiles adjusted during validation

## Explicitly post-beta / post-v1 expansion

- NES/SNES/Mega Drive/PS1
- RetroArch compatibility build
- Themes and richer visual customization
- Artwork/metadata databases
- Advanced networking/services
- Extra emulator expansion

## Beta 1 release decision

On 2026-10-11 the remaining hardware-soak items were explicitly accepted for the
first public Beta so broader community testing can begin. The following are
documented in `docs/BETA1_KNOWN_ISSUES.md` and remain active Beta work:

- broader real-device RTC game regression;
- final Wi-Fi visual/interaction coverage across more networks;
- deliberate updater interruption/rollback torture testing;
- longer suspend soak and idle-drain characterization;
- extended reboot/recovery torture coverage;
- broader post-freeze GB/GBC/GBA regression and log sweeps.

The release is intentionally labelled **Beta 1**, and users are asked to report
all issues through GitHub Issues.

## Beta gate

Do not call a future build stable until its release-blocking items have either
been completed or explicitly accepted and documented for that release.
