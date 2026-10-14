# RK3566 legacy kernel module provenance

**Recorded:** 14 October 2026. The current Beta 6.1 release pipeline copies four
prebuilt `.ko` modules out of the archived
[v0.1.0-beta.4 update package](https://github.com/AnthonyWCurtis/FlipOS/releases/tag/v0.1.0-beta.4).

The successful [Beta 6.1 publish job](https://github.com/AnthonyWCurtis/FlipOS/actions/runs/38516999429)
printed SHA-256 digests **after copying those modules** into `build/`:

| Module | SHA-256 |
| --- | --- |
| `flip_opp_voltage.ko` | `bceb39b3ece552a2fc81170a9f15bc5e1e181044e8b1a517318c61a5e2f2cf0a` |
| `flip_opp_runtime_profile.ko` | `da6b696925b5f1eb7eef6335d7459ecde1286568b921ca1672e2e71ab438e79b` |
| `flip_tsadc_recovery.ko` | `9dc4bb1965cd8eee8aeee0bceedb5d824a6e42d371c63e51131bb14e0f3a2d90` |
| `flip_rk817_charger.ko` | `ee2b58290d49a62602435086c39d28bbd9c4fb0145fa81034dab413780050bb4` |

The PR package smoke test now pins the **entire Beta 4 update archive** by
SHA-256, verified *before* passing it to tar:

- `FlipOS-Update.tar.gz`: `0a77452b41737c489244a29e4502506908d07497c2c3a0d75c10b681128cd68f` (GitHub's published release asset digest)
- The single-source lock is `tools/kernel/legacy-module-lock.json`; the
  verifier is `tools/kernel/verify_release_module_inputs.py`.
- The CI job extracts **only** the four known module members, then verifies
  every copied module's SHA-256, AArch64/ELF64 relocatable format, module name,
  GPL-compatible license, embedded 5.10.160-based `vermagic`, and consistent
  `vermagic` across all four modules.
- `make verify` independently runs eleven synthetic offline tests, including
  archive mismatch, changed modules, wrong architecture, wrong kernel version,
  malformed ELF and differing module metadata.

**Proof boundary:** Kernel `vermagic` and byte hashes are **necessary but not
sufficient** to prove the modules are safe to load. A module compiled with the
same release string may still reference incompatible kernel symbols or disagree
with kernel configuration/ABI. Also the lock records existing Beta 4 release
**bytes**, not the reproducible source/toolchain used to generate them.
Physical-device verification requires separately collected `uname -r`,
`/proc/version`, kernel config, module dependency / symbol CRC evidence and
safe no-load checks. Do not load/reload any module merely to prove compatibility.

The archive is downloaded from GitHub Releases; pinning a hash taken from that
release prevents accidental content drift but does not independently authenticate
a compromised publisher. Keep release supply-chain trust as a separate gate.

## Still required for an independent, trusted release

- Verify modules came from reviewed corresponding source changes and a known
  Rockchip 5.10.x kernel/toolchain tree.
- Capture `modinfo -F vermagic` (or equivalent ELF/kernel inspection) for each
  module; compare against the exact kernel booted on target Miyoo Flip boards.
- Check module dependencies and that any required ABI/config exports match.
- Rebuild the modules reproducibly or pin their source/toolchain inputs, not
  merely a release download.
- Add equivalent digest and ABI checks to a **future, deliberately versioned**
  release workflow. **Do not edit the current Beta 6.1 publish workflow simply
  to change CI**, because its GitHub Actions push-path trigger can republish
  the historical Beta 6.1 release.
- Continue physical-device charging, thermal, recovery and voltage-safety
  acceptance independently; never load an unknown `.ko` just to test it.

Never change internal NAND/MTD while validating these modules.
