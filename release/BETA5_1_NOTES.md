# FlipOS v0.1.0-beta.5.1

## Wi-Fi hotfix

Beta 5.1 is a focused prerelease hotfix for Wi-Fi setup issues found during physical testing on the Miyoo Flip.

### iPhone / personal hotspot SSIDs
- Fixes hotspots whose SSID contains non-ASCII characters such as the curly apostrophe in **Anthony’s iPhone**.
- `wpa_cli scan_results` can expose those bytes as printable `\\xNN` escapes. FlipOS now decodes them back to the real SSID bytes before storing the network or deriving its WPA PSK.
- This prevents the hotspot name shown in the browser from differing from the SSID used for authentication.

### Wi-Fi password keyboard
- The on-screen keyboard now has drawable glyphs for the complete printable ASCII punctuation set used by WPA/WPA2 passphrases.
- Symbols such as `@`, `#`, `$`, `%`, `&`, quotes, brackets, backslash, `?`, `=`, `_`, `|`, and `~` are visible and enterable.
- Uppercase letters remain available through the existing case toggle.

### Regression coverage
- Added a test for an iPhone-style smart-apostrophe hotspot SSID.
- Added a keyboard test requiring the complete printable ASCII password character set.
- Added a UI architecture guard requiring punctuation keys to have drawable glyphs.
- Wi-Fi tests are now part of focused CI and the release gate.

## Also included

Beta 5.1 contains all Beta 5 changes, including:
- the 640×480 console UX redesign;
- Quick Menu and GameSwitcher controls;
- the new Power Menu;
- smarter dynamic CPU rescue for visible frame-pacing drops such as the FFVI issue reported on Beta 4.

## Downloads

- **FlipOS-Fresh_Install.zip** — fresh SD-card installation.
- **FlipOS-Update.tar.gz** — update an existing FlipOS installation.
- **FlipOSUpdate.tar.gz** — legacy updater-compatible filename.

Each archive includes a SHA-256 sidecar.

This remains a **prerelease**. Physical Wi-Fi testing is especially useful for this hotfix.
