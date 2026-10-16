# FlipOS USB mode switch — ADB by default, MTP only when selected

**Experimental, 16 October 2026. Do not deploy automatically.**

## User behaviour

- Normal boot leaves Rockchip's stock **ADB-only** USB gadget running.
  Unlike the previous boot path, it does not start `umtprd`, stop
  `usbdevice`, unlink functions, or rebind USB.
- FlipOS Settings → Connectivity → USB File Transfer offers:
  **ENABLE FILE TRANSFER (MTP)** and **ADB ONLY (DEFAULT)**.
  Press A once to see a warning, A again to request the transition.
  Press B to cancel. The user must finish any PC transfer first.
- Switching to file transfer enables the existing limited ROMs/BIOS/Saves
  composite, with ADB still present. Changing USB gadget functions
  **briefly disconnects ADB**, and Windows must re-enumerate it.
- Returning to ADB removes only the MTP function link while keeping the ADB
  link; the MTP daemon might remain alive until reboot. There is no promise
  of seamless USB connection during mode change.
- A reboot always aims for stock ADB-only. If the gadget state is unexpected
  at boot, the helper reports it without risky automatic USB modification.

## Safety

`tools/device/flip-usb-mode status` is read-only. `boot` is a
non-mutating check except optional diagnostics log. `mtp` and `adb`
are explicit, guarded actions: reject unbound/missing gadget, missing ADB,
non-configured USB, active emulator/updater/optimizer, concurrent transitions,
and unknown function layouts. Failure during a mode switch may still require
a normal reboot. USB power delivery is independent of the USB gadget mode;
nothing here changes RK817 charger current limits or battery safety.

**Important:** the existing `flip-usb-mtp` helper reconfigures the gadget
and stops stock `usbdevice`. It requires careful physical A/B testing on the
Miyoo Flip before we consider it release safe. ADB mode may remain without
the stock helper's event supervision after a round trip until the next boot.
Disabling MTP while the PC is transferring a file can interrupt that file;
human confirmation is necessary and insufficient as automatic proof of idle
transfers. We must evaluate whether to expose Saves for MTP while game data
can be written; the runtime guard helps, but is not filesystem locking.

## Validation and rollout

1. GitHub CI runs 13 synthetic temporary configfs/sysfs test cases,
   static UI/boot architecture checks, the full `make verify` suite,
   native AArch64 frontend build and release package dry run.
2. Reconcile the user’s **77 unpublished files** from
   `~/projects/FlipOS` into a new worktree rather than resetting them.
   We created an isolated local worktree at
   `~/projects/FlipOS-usb-modes-20261016`, but that clone is not yet
   updated with the GitHub draft’s code; it must be integrated deliberately.
3. Only with explicit physical-device consent, back up SD saves and
   existing `updater-playtest` and scripts, deploy a test-only package.
   ADB-only reboot and ADB detection first; no immediate mode toggle.
4. Verify ADB-only for 30–60 minutes idle/connected, then one brief attended
   lid/sleep cycle and reconnect checks. Avoid transfers and active games.
5. Enable File Transfer in Settings, verify ROMs/BIOS/Saves, then switch back
   with NO active PC transfer. Monitor both WinUSB driver and Rockchip DWC3.
   Unexpectedly unbound gadget or loss of host enumeration means stop.
6. Repeat an ADB-only baseline after normal reboot. Compare ADB-only vs MTP
   idle disconnects, pstore/dmesg, charging and battery logs. If either mode
   fails, return to stock boot for safety and investigate DWC3/FunctionFS.

**Blocked:** no real-device MTP switching has been performed by this PR.
Kernel USB robustness remains unresolved. Do not merge or ship until
we have hardware evidence and a safe rollback process.
