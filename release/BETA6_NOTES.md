# FlipOS v0.1.0-beta.6

## Safe CPU optimization and shared GPU rendering

Beta 6 focuses on performance, power efficiency and runtime reliability on the Miyoo Flip.

### Full-range CPU optimizer
- CPU Optimization now calibrates the full RK3566 CPU frequency range instead of only the upper performance levels.
- The production optimizer keeps a 25 mV safety margin and does not tune below the supported 812.5 mV floor.
- The validated profile is applied dynamically when games launch and restored safely when they exit.
- Stability validation covers all 8 CPU operating points.
- The Results screen now shows every CPU operating point and reports **VERIFIED 8/8** after a complete successful validation.
- Recovery protection remains active during calibration and validation so unsafe trials fall back to stock-safe settings.

### Shared GPU video host
- Fixes the shared Mali GPU host startup handoff with an explicit readiness handshake instead of timing-based socket polling.
- Fixes in-game Quick Menu rendering while the shared GPU video path is active.
- Keeps GBA/GB/GBC gameplay on the GPU-backed video path instead of silently falling back to the CPU when startup timing varies.
- The GPU host service is now explicitly included in both fresh-install and update packages and covered by release gates.

### Dynamic performance
- Dynamic CPU scaling can raise the performance floor when frame pacing degrades and lower it again after sustained healthy playback.
- GPU frequency remains dynamically managed across the supported performance range.
- The goal is to spend less time at unnecessarily high clocks while retaining smooth gameplay.

### Power button
- The RK817 PMIC power key is read from its dedicated input device, fixing short-press Power Menu detection while leaving the hardware emergency long-hold behavior independent.

### Wi-Fi
- Saved-network handling supports multiple remembered networks and improved switching/diagnostics.
- Includes the Beta 5.1 fixes for escaped/non-ASCII hotspot SSIDs and the complete printable password keyboard.

## Hardware validation

Development hardware testing for this release included:
- GBA, GB and GBC gameplay;
- Golden Sun: The Lost Age GPU/performance testing;
- shared GPU Quick Menu rendering;
- dynamic CPU/GPU behavior;
- full 8-OPP CPU optimizer calibration and Standard stability validation;
- Power Menu input;
- sleep/wake and GameSwitcher flows exercised during development.

The experimental below-spec CPU undervolt research is **not included** in Beta 6.

## Downloads

- **FlipOS-Fresh_Install.zip** — fresh SD-card installation.
- **FlipOS-Update.tar.gz** — update an existing FlipOS installation.
- **FlipOSUpdate.tar.gz** — legacy updater-compatible filename.

Each archive includes a SHA-256 sidecar.

This remains a **prerelease**. Back up your SD card before installing or updating.
