# FlipOS work-to-home handoff — offsite verification bundle

**Prepared 16 October 2026. No device testing performed. Not a release or installation.**

## Already prepared from work

- This isolated integration branch combines draft stability PR #49 and charger PRs #50–#53.
- GitHub CI runs the combined native tests, offsite updater/save models,
  diagnostics privacy, charging selector counterexamples, battery sensor
  fail-closed checks, manual read-only inspection, and daemon identity checks.
- `tools/device/flip-charge-collect-adb.py` is a new **PC-side** tool. It requires
  explicit `--consent`, accesses one authorised ADB device only, and executes a
  fixed shell script that performs read-only `cat/head`, `grep`, `uname`, and
  `sha256sum` operations. The shell script is passed over ADB stdin; no
  on-device install, log deletion, PMIC write, service restart or package push.
- The ADB tool only saves filtered numeric/status/module-hash fields to a
  **new** private host-side JSON file. Existing files are not overwritten. It
  never uploads the report. Status reads do **not** prove charge thermal cutoff.

## When the MSI reconnects — preserve source first

Do not run `git reset`, `git clean`, `git stash`, `git pull` or checkout
another branch in the user's active `~/projects/FlipOS` working directory.
It has unpublished changes, including Save Manager and checkpoints.

In a new WSL terminal, a safe *read-only* initial inventory is:

```sh
cd ~/projects/FlipOS
git branch --show-current
git rev-parse HEAD
git status --porcelain=v1
git worktree list
```

The lead developer should capture these and reconcile the latest MSI source
in an **isolated new worktree** rather than applying the older GitHub draft
directly over dirty files. This remains an engineering review, not a deployment.

## One-command room-temperature ADB observation

When physically with the Miyoo Flip, finish/close any active game saves and
updates first. Connect the handheld via an authorised USB ADB connection and
run the following **from a worktree containing this integration branch**:

```sh
adb devices
python3 tools/device/flip-charge-collect-adb.py \
  --consent \
  --samples 6 \
  --interval-seconds 10 \
  --output ~/flipos-charge-idle-plugged.json
```

The command takes roughly one minute, reads only allowlisted paths, and writes
a local JSON file on the PC. It has no auto-upload. Use a different output
filename for a second observation. If WSL's `adb` cannot see the device,
configure a local authorised ADB executable via `--adb`; do not run the
collector using an unrelated phone or device. The utility rejects zero or
multiple attached authorised devices unless `--serial` is explicitly supplied.

For an initial safe baseline:
1. At ordinary room temperature, idle unplugged for a short attended sample.
2. Idle plugged in, attended, for a separate short sample.
3. Only later, with explicit approval and **disposable game saves**, compare
   GB/GBC/GBA gameplay with and without external power and short lid cycles.
   Do not unplug during an updater, optimizer session or save operation.
4. Store reports on the host for review. Report is **not evidence** of thermal
   cutoff, cell safety, kernel lock ownership or actual loaded `.ko` identity.

**Never intentionally overheat/cool the battery, force charging at 0°C/50°C,
write raw RK817 registers, alter CHRG_EN/power path, or stress-test unattended.**

## Still physically required (cannot be completed responsibly from work)

- Verify installed `flip_rk817_charger.ko` file SHA/vermagic plus stock kernel
  `5.10.160` driver and device tree. A matching SD file hash is **not** proof
  of which module is actually loaded.
- Confirm RK817 thermistor TS wiring, register protection and **stock** charger
  ownership; do not infer from unrelated upstream/mainline driver source.
- Hardware acceptance for low-battery shutdown, lid sleep, thermal behaviour,
  charge while playing, actual current and no unintended PMIC resets.
- Reconcile newer MSI changes, rerun full tests and accept release evidence on
  a final exact source commit and package hash.

**Ship policy:** keep these drafts unmerged. Keep stock charging authoritative
until safe hardware evidence supports any additional FlipOS charge controller.
