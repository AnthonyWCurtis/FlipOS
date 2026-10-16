# FlipOS combined stability + charging offsite integration — 16 October 2026

**Engineering staging branch only. NOT merged, installed, hardware tested or approved for release.**

## Scope and provenance

This branch starts at exact draft stability-rollup PR #49
`9d7dafc55bdc9d3417bd403ff9c18d6fea2c4758` (itself an integration of draft #43–#48) and composes the
reviewed blobs from charger PRs #50–#53, without rebasing or modifying
any of those source branches.

- **#50 Charging audit:** six synthetic examples exposing unsafe current
  selector/restore sequences, an engineering report. Passing these model tests
  **reproduces known hazards** — it does not establish charger safety.
- **#51 Userspace hardening:** physical-sensor battery snapshot for charge
  service; on missing readings retain previously applied caps rather than
  release, and do not release cap after helper write failure. Host C tests.
  Does not change `flip_rk817_charger.ko` or prove hardware thermal cutoff.
- **#52 Manual read-only inventory:** exact allowed status and module SHA paths,
  seven synthetic tests. Not installed into FlipOS runtime.
- **#53 Bounded observation & PID identity checks:** thirteen host tests and
  a Python observer, again **not** an installed runtime service. Never auto
  restart another charging daemon based on a stale or uncertain PID.

Combined CI preserves PR #49's 46 Python validation/model/privacy scenarios,
adds the 6 + 7 + 13 charging scenarios, and continues `make verify`, ARM64
userland compilation and the package dry run. Total extra Python scenarios:
**72**, counting six release-safe-model suites by their currently declared
test methods, not excluding the baseline `make verify` suites.
[Review note: use the CI logs for exact test counts.]

## Hard blockers not resolved by this branch

1. Kernel charger ownership is **unverified** on the exact Miyoo Flip board.
   Stock Rockchip `rk817_charger` driver writes and our helper's raw
   `PMIC_CHRG_OUT 0xE4` full-byte accesses are not coordinated. The
   saved `restore_selector` can restore a higher old current after a newer
   vendor restriction. No speculative PMIC register writes are added here.
2. `PAUSED_COLD`/`PAUSED_HOT` request selector **500 mA**, not zero.
   Hardware TS thermistor wiring and actual autonomous cutoff are not proven.
3. Current packaging pins an older Beta 4 charger `.ko` with locked SHA-256
   and kernel `5.10.160` vermagic. Exact loaded module identity is unknown.
4. Shell launcher relies on `kill -0` with a PID file, which alone cannot
   prove charge daemon identity or health. The observer diagnoses this;
   do **not** auto-restart without a safe single-owner design.
5. Actual MSI checkout has newer uncommitted changes; all draft code from
   GitHub is older than that tree. Preserve local modifications and
   untracked files; reconcile in a **new, isolated** worktree before any merge.
6. No proof of power-cut-safe save/update multi-file commit or real handheld
   boot/suspend/charge-while-playing acceptance. Keep the existing release
   gate fail-closed.

## Safe next steps

- Finish clean CI in this branch and leave this as a **draft PR**. No release.
- When MSI reconnects, inventory the dirty source and local kernel metadata,
  not a destructive checkout/pull. Keep the existing audit worktree intact.
- Verify the board stock driver and RK817 TS/charger device-tree wiring,
  locked `.ko` SHA and process identity with only approved **read-only**
  collection on device.
- Use the prepared ordinary-room-temperature test matrix with disposable
  game saves. No deliberate extreme-temperature charging tests, raw I²C writes,
  NAND/MTD changes, experimental undervolting or unattended hardware runs.
- A qualified code review should decide whether enhanced RK817 current
  overrides are safe at all; default to stock charging if ownership is unclear.
