# FlipOS — future stable release acceptance contract

*Draft engineering gate, 16 October 2026. Current published version is beta. This is **not** a stable-release approval.*

## Principle

`docs/ROADMAP.md` remains the canonical scope/priority document. `docs/BETA_READINESS.md` preserves historical Beta 1 evidence. This new gate is intentionally **release-specific**: no earlier beta's checkboxes, model tests, or green CI job automatically approve the next stable candidate.

A passing host/ARM64 CI run verifies builds and automated tests, but **does not** prove save safety across a power cut, physical device sleep/wake stability or battery-life claims. The exact build intended for distribution needs fresh acceptance evidence.

## Independent release identity

Record the full 40-character source commit, the literal version string from the candidate's VERSION file and hashes of the exact update archive and fresh-install ZIP **after** packaging. The checker requires the independently supplied `--version` and `--source-sha` to match the manifest and verifies artifact bytes against the hashes. A fabricated manifest or evidence document may still pass structural checks; human review of the evidence and actual device behaviour is mandatory.

Use `docs/release/RELEASE_EVIDENCE_TEMPLATE.json` only as a **pending sample**. Its zero hashes and placeholder version/commit are deliberately unusable as a release approval. Copy it outside the repository with the evidence/artifacts on your trusted development PC. **Do not commit ROMs, identifiable logs, Wi-Fi details or personal files.** Use short, anonymised test results and hashes. Evidence paths are local to the selected `--root`, are bounded in size and must be regular non-symlink files. These checks are not a substitute for private-file handling or adversarial filesystem guarantees.

Example for a candidate prepared and approved by a release engineer:

```sh
python3 tools/release/release_readiness.py \
  --manifest /path/to/private-evidence/manifest.json \
  --root /path/to/private-evidence \
  --version 0.1.0-rc.1 \
  --source-sha 0123456789abcdef0123456789abcdef01234567
```

Strict mode returns nonzero if **any** gate, hash, schema or identity check fails. For a non-blocking status report only, add `--audit`; **a successful audit exit never indicates release approval**.

## Required gates (all must have reviewed, matching evidence)

| Gate ID | Mandatory physical/release acceptance |
| --- | --- |
| `integrated_source_ci` | All intended PRs reconciled with latest MSI checkout; `make verify`, host/ARM64 CI and chosen source identity pass |
| `release_artifacts_modules` | Update and fresh-install artifacts rechecked, kernel module SHA-256 / provenance / ABI-vermagic validated for exact candidate |
| `native_gb_gbc_gba_regression` | Post-feature-freeze gameplay, performance and system controls across GB, GBC, GBA on device |
| `sram_state_rtc_integrity` | Real saved game, RTC and savestate recovery, no loss of game progress; preserve known-good backups |
| `save_restore_interruption` | Disposable-save restore/undo failures, corruption, resume blocking and interruption matrix |
| `updater_powerloss_recovery` | Genuine interrupted-update safe restart, validation and rollback on disposable SD with trusted boot fallback |
| `sleep_lid_resume_soak` | Repeated lid close/open, deep suspend and prolonged sleep/idle drain with stable resumes |
| `gpu_cold_start_frame_pacing` | Cold shared-GPU start, return-to-game, FPS and scanout wait sanity across target cores |
| `audio_volume_resume` | RK817 audio after deep resume; volume including 0% mute and first increase from zero |
| `battery_charge_thermal` | Unplugged real energy use, stable charging while gaming and thermal controls |
| `wifi_setup_time_rtc` | First-boot Wi-Fi wizard, reconnect, secure credentials, timezone and RTC game clock accuracy |
| `diagnostic_review_and_privacy` | Crash/error/log sweep, no unintended collection and opt-in export reviewed for privacy |

**Dangerous CPU voltage optimiser research remains excluded from this release gate until separately proven safe; passing these checks does not enable it.** Additional emulators and RetroArch remain deferred.

## Integration sequence for current draft GitHub work

1. **PR #43**: port the resume-block ordering/test against MSI's updated checkpoint implementation. Verify marker creation and atomicity boundaries; never blindly apply the older public-main patch.
2. **PR #44**: integrate the upfront rollback ledger after checking the newer MSI installer and packaging. This alone does **not** guarantee crash recovery.
3. **PR #45**: use the host-only power-loss model as an implementation specification, not a shipped boot recovery feature. A trusted recovery executable and physical SD tests are separate blockers.
4. **PR #46**: opt-in developer-host metrics exporter; verify filenames, privacy and parser behaviour. No automatic upload service is authorised.
5. **PR #47**: update the dynamic CPU policy invalid-FPS test after reconciling local edits. Monitor actual clock choices and current draw before claiming savings.

All five remain **draft** until latest MSI source and physical device acceptance. Do not merge the drafts into `main` as a batch. Bring each into an isolated worktree, resolve conflicts, run `make verify`, review diffs and then hardware-test the appropriate change before an individual merge decision.

## What this automation can and cannot assert

- The checker validates **structure and cryptographic file hashes**, not whether a test was actually performed or was truthful. `passed` requires at least one matching, locally available proof file; a text note can still be fabricated.
- CI executes the **checker regression tests**, not an approval for the current Beta or a future stable build. All release fields start pending. It is intentionally **not** wired to a release publishing workflow yet; freeze the integration and hardware acceptance contract first.
- Neither this document nor a passing PR check licenses device flashing, use of active game saves, automatic firmware deployment, undervolting or publication as stable.