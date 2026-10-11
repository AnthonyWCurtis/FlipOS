# FlipOS v0.1.0-beta.2

**Beta 2 — first public update / hotfix release.**

This release fixes two issues discovered during a clean SD-card installation of
Beta 1.

## Fixed

### First-boot time sync could appear frozen

On the Date & Time page, continuing setup could trigger a blocking one-shot NTP
sync while Wi-Fi and DHCP were still settling. On real hardware this could take
tens of seconds and make the setup screen look stuck.

Beta 2 now lets normal automatic NTP synchronization continue in the background.
Pressing **A / Continue** no longer performs a blocking sync.

The optional **X / Sync** action is still available, but its explicit NTP
attempt is now bounded so it cannot leave the frontend waiting indefinitely.

### CPU Optimizer could show START FAILED on a fresh SD card

A completely clean installation has no optimizer state directory yet. The
optimizer tried to create its rollback snapshot before that directory existed,
so the first optimization attempt could fail immediately.

Beta 2 creates the optimizer state directory before taking the initial snapshot.
The empty first-run snapshot and restore path has also been verified on the
Miyoo Flip.

## Updating from Beta 1

Beta 1 already contains the FlipOS updater.

Connect to Wi-Fi, open the FlipOS update screen, check for updates, and install
Beta 2. The update package is validated and staged before installation.

As this is still Beta software, please keep normal backups of important saves.

## Fresh install

A fresh-install ZIP is also provided for new users.

## Please report issues

FlipOS remains in public Beta. If you find a crash, failed resume, game issue,
Wi-Fi problem, optimizer problem, battery/power issue, update problem, or
anything else that looks wrong, please report it:

https://github.com/AnthonyWCurtis/FlipOS/issues
