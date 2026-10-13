# FlipOS Saved Wi-Fi Networks (Beta 5.1 feature branch)

## Storage and migration

The source of truth is the persistent file
/mnt/SDCARD/flipos-dev/state/network/wpa_supplicant.conf.
It now contains up to 16 network blocks; no additional database
or background daemon is required. A legacy config containing one block and
no priority field is imported as-is on first update. Unknown directives
make the profile operation fail safely rather than silently deleting data.

Only WPA's derived 64-character hexadecimal PSKs are saved, never plaintext
passphrases. The directory is mode 0700, the atomically replaced config mode
0600. Open networks use key_mgmt=NONE.

## Behaviour

- Connecting to a new network retains existing saved networks.
- Selecting a saved, secured SSID connects without asking for a password again.
- The newly connected network is staged at the highest priority.
- If association to the requested SSID fails, the previous profile set is
  restored and the supplicant is restarted with those previous networks.
- Successful association is retained even when DHCP temporarily fails.
- After an automatic connection gains a usable IP, the saved priority order
  is updated on disk so the proven network is preferred on subsequent
  selections and restarts.
- The normal Wi-Fi Y button forgets the selected visible SSID rather than
  removing the whole config. Other profiles remain available.
- If WPA is absent at frontend startup and saved profiles are present,
  the frontend status refresh can start WPA to auto-connect.
- Scanned saved networks show an SV indicator.

The Wi-Fi diagnostic tool supports saved (SSID and rank, no secrets) and
forget <ssid> for removing an out-of-range profile. A dedicated
on-device Saved Networks list can be added separately.

## Validation

Host-side:

1. make test-wifi (legacy import, open/secured, escaping, priority,
   updating, selective forget, 16-profile limit, malformed config safety).
2. make test-architecture (includes Wi-Fi and frontend architecture).
3. make frontend wifi-tool (cross-compiled AArch64 binaries).

Physical Miyoo Flip acceptance checks before shipping:

1. Back up the existing Wi-Fi config from the SD card.
2. Connect to home Wi-Fi; verify it gets an IP.
3. Connect to an iPhone hotspot; verify both remain saved (SV).
4. Disable the hotspot and verify the Flip returns to home Wi-Fi without
   entering a password; reverse the test by disabling home Wi-Fi.
5. Reboot at both locations and verify auto-connect.
6. Re-select a saved protected SSID and verify the keyboard is skipped.
7. Try a wrong password on a new test SSID, confirm previous profiles
   remain, and verify automatic connection still succeeds.
8. Press Y on the selected hotspot and confirm home remains saved.
9. Check Wi-Fi/DHCP behaviour after lid close/open and extended idle.

This implementation has compile/test coverage but has not yet passed
physical-handheld acceptance. Do not publish a release solely on the
strength of the host tests.
