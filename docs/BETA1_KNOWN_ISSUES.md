# FlipOS Beta 1 Known Issues

FlipOS Beta 1 is an early public test release. Bugs and rough edges are expected.

Please report issues at:

https://github.com/AnthonyWCurtis/FlipOS/issues

## Known limitations

### Save Safety restore/undo is temporarily hidden

FlipOS continues to create normal save backups internally, but the experimental **Save Safety / Restore Previous / Undo Restore** user interface is intentionally disabled in Beta 1 while that workflow receives more hardening.

Your normal game saves still work, and the `Saves` folder is available over USB for manual backups.

### Shared GPU host is disabled for Beta 1

A shared GPU presentation path caused a multi-second black-screen delay on some game launches. Beta 1 deliberately uses the proven direct CPU/DRM presentation path instead.

This avoids the launch timeout and is the intended Beta 1 configuration.

### Long suspend and idle drain are still being characterised

Lid sleep/wake has been hardware-tested, but very long suspend periods and idle battery drain still need broader real-world testing across devices.

Please report unexpected wakeups, excessive battery drain, or failed resumes.

### Update interruption testing is still ongoing

The updater validates packages and keeps rollback data, but Beta 1 users should still avoid deliberately interrupting installation or removing power/SD media during an update.

### RTC support needs broader game coverage

Hardware RTC, timezone handling, NTP/manual time and GB/GBC RTC persistence have been exercised on real hardware. More game-specific and long-duration RTC testing is still welcome during Beta.

### CPU optimisation is still Beta

Per-device CPU optimisation contains guarded recovery paths and can fall back to stock-safe operation. Nevertheless, hardware variation is exactly why this feature needs public Beta coverage.

If you see crashes, reboots, validation loops, or unusual performance after optimisation, report the issue.

## Scope

Beta 1 intentionally supports only:
- Game Boy;
- Game Boy Color;
- Game Boy Advance.

Other systems are planned for later releases.

## Save filename change

Older FlipOS development builds used hashed save filenames. Beta 1 uses names derived from the ROM filename so saves are understandable when viewed over USB.

When an older hashed save belonging to a game is found, FlipOS migrates it automatically when that game is loaded.

## Reporting a useful bug

Please include:
- FlipOS version;
- console/system and ROM name;
- exact steps leading to the issue;
- whether it followed a cold boot, reboot, lid resume, or GameSwitcher action;
- whether the problem repeats;
- photos/video where useful.

Thank you for testing FlipOS.
