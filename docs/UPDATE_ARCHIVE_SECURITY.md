# FlipOS update archive safety (next release candidate)

FlipOS currently distributes `FlipOS-Update.tar.gz` alongside a matching
`.sha256` sidecar on GitHub Releases. The updater download path should never
interpret unverified archive headers on removable SD media.

## Incoming update validation

For **full downloads** (not background metadata-only checks), the future
fetcher in this branch:

1. Reads the official update asset URL from GitHub release metadata.
2. Downloads the archive and its companion SHA-256 sidecar.
3. Confirms the sidecar describes the expected asset basename and compares
   it with the *downloaded archive's* full SHA-256.
4. Enforces a 64 MiB compressed download cap and 128 MiB decompressed TAR
   cap. Each member is limited to 32 MiB; the member count is bounded.
5. Decompresses with a shell file-size resource limit, then inspects each
   original 512-byte TAR header using portable `dd`, `od` and `awk`.
6. Rejects malformed checksums/headers, traversal, non-ASCII or unknown
   paths, duplicates, hardlinks, symlinks, devices and extension headers.
   Only the documented FlipOSUpdate manifest/checksums/payload layout is
   allowed. GNU tar/USTAR simple filename format is supported; PAX and
   long-name extension records deliberately fail closed.
7. Calls BusyBox `tar -xf` **only after** successful header preflight.
8. Retains the existing manifest target and per-file SHA-256 checks. The
   installer separately restricts target paths and symlinks.

These caps are meant to exceed present FlipOS package needs; benchmark them
against the actual next release size. Metadata-only `--check-only` never
downloads an archive or sidecar.

## Important security limits

- A checksum from the **same GitHub release** is an integrity check, **not an
  independent signature or trust anchor**. Compromise of the release publisher,
  API credentials or distribution origin could replace both files. Consider
  signing immutable release manifests with an offline/private key and pinning
  a verification public key in future images.
- Archive preflight is intentionally conservative and can reject some generic
  TAR encodings. Keep the production package builder to simple short-name
  GNU TAR/USTAR entries or explicitly version any future format migration.
- Real-device BusyBox `sh`, `awk`, `od`, `dd`, `gzip`, `tar`, file
  limits, free space and package-transfer latency **must** be validated on
  the physical Miyoo Flip before publishing this change as an update.
- Verify the existing Beta 6.1 installer/update path and rollback against
  controlled power-loss and interruption tests; CI does not replace these.
- This change is source code for a **future** release. Existing published
  Beta 6.1 binaries are not retroactively modified, and no internal NAND/MTD
  modifications are part of this work.

## CI test coverage

`tests/test_update_fetch.py` covers normal download, metadata-only checks,
mismatched/malformed sidecars, disallowed paths, link/device entries,
duplicates, malformed TAR headers and oversized decompressed archives.
`tests/test_update_installer.py` covers allowed installs, tampered payloads,
path restrictions and symlinked installation boundaries.

A full `make verify` pass means the native host and cross-build tests ran,
**not** that any unsafe-voltage, on-device GPU or battery acceptance passed.
