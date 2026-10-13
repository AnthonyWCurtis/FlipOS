# FlipOS v0.1.0-beta.5

## Console UX test release

Beta 5 is the first full pass at making FlipOS feel like a purpose-built handheld console rather than a collection of utility screens.

The main goal of this prerelease is **physical UX testing on the Miyoo Flip's 640x480 display**. The layouts now use explicit pixel geometry and measured text widths so screens can be checked automatically before they ever reach the device.

## What's new

### New Home
- Game-first **CONTINUE** hero for the most recent game.
- Play time, launch count and last-played information on Home.
- Recent-game cards.
- Secondary Library, GameSwitcher, Play Stats and Settings navigation.
- Long game names are measured and fitted instead of spilling into nearby UI.

### Redesigned GameSwitcher
- Selected game gets a large hero card.
- Clear PAUSED / RESUME / RESTORE state.
- Play statistics for the selected game.
- Three-card recent-session strip.
- Left/right and up/down navigation.

### Redesigned Library
- Consistent system and game cards.
- Measured game/system names.
- Cleaner focus state and control hints.
- Existing launch and Game Details behaviour retained.

### Play Stats and Game Details
- Console-style summary cards.
- Per-game history remains stored locally on the SD card.
- Play time, launches, session history and played dates are presented with fixed bounds.

### Settings family
The new visual language now carries through:
- Settings
- Connectivity
- Wi-Fi browser
- Wi-Fi password entry
- System settings
- Date & Time
- BIOS Verifier
- System Update
- CPU Optimizer and Optimizer Results
- First-boot setup

### Quick Menu
- Tap **MENU** to open the in-game Quick Menu.
- Hold **MENU** for about 700 ms to enter GameSwitcher.
- The game is paused behind a dimmed frozen frame.
- Save State, Load State, volume, brightness, Performance HUD, GameSwitcher and Quit Game remain available.
- **Quit Game returns to Home.**

### Controls cleanup
- **START** and **SELECT** belong to games.
- **START + SELECT** has no FlipOS system action.
- **MENU + SELECT** has no FlipOS system action.
- The old MENU+SELECT shutdown gesture has been removed.

### New Power Menu
A short press of the physical **POWER** button is now designed to open:
- Sleep
- Restart
- Power Off

Restart and Power Off require confirmation. In-game destructive power actions checkpoint the current game first; a failed save cancels the power action.

A long physical POWER hold is deliberately left to the hardware/emergency power path.

> **Beta 5 test note:** the POWER-button software path is newly mapped to Linux KEY_POWER on the Miyoo Flip digital input device. It has passed build, unit and architecture tests, but this prerelease is its first physical-device validation. Please specifically test short POWER press, Sleep/wake, Restart and Power Off.

## Pixel-layout guardrails

The redesigned screens are authored against the real **640x480** framebuffer. FlipOS now has automated layout checks for Home, GameSwitcher, Library, Play Stats, Game Details, Settings, Wi-Fi, Update, BIOS, Date & Time, first boot, CPU Optimizer, Quick Menu, Play Options and Power Menu.

The built-in font advances exactly 6 pixels per character per scale, so dynamic strings can be measured, truncated or scaled intentionally instead of relying on framebuffer clipping.

## CPU Optimizer

Beta 5 keeps the autonomous optimizer and validation safeguards introduced in Beta 4. This UX work does **not** replace the device-owned voltage-search logic.

Existing calibrated/verified profiles remain device-specific. Runtime tuned voltages are still validation-gated, and failed/quarantined profiles continue to fall back to safe settings.

## Downloads

- **FlipOS-Fresh_Install.zip** — fresh SD-card installation.
- **FlipOS-Update.tar.gz** — update an existing FlipOS installation.
- **FlipOSUpdate.tar.gz** — legacy updater-compatible filename.

Each download is accompanied by a SHA-256 sidecar.

## What to test

Please pay particular attention to:
- text or cards touching/overlapping on the physical 640x480 display;
- Home Continue behaviour;
- GameSwitcher resume/suspend flow;
- Library navigation and long ROM names;
- Quick Menu tap/hold MENU behaviour;
- POWER short press versus long hardware hold;
- Sleep/wake from the Power Menu;
- Restart and Power Off confirmations;
- Wi-Fi/password entry;
- first-boot pages;
- CPU Optimizer screens;
- Quit Game returning to Home.

This remains a **prerelease**. Please report anything that looks wrong, even if it is purely visual.
