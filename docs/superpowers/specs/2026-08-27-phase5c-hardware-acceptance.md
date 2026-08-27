# FlipOS Phase 5C Hardware Acceptance

Date: 2026-08-27
Device: Miyoo Flip, RK3566
ROM used: `/mnt/SDCARD/Roms/GB/Pokemon - Yellow.gb`
ROM identity: `792f9628ba43b7d9`

## Candidate

Final Phase 5C hardware-tested code includes the automatic lid-resume regression fix at commit `593eb53e54334fae67bd377b6f02bab107436ac2`.

Final deployed binary hashes:

```text
flip-gb       537eb2518739aa7ae947e56ce7aef5805845a58eda8d995373c2f48b194337cd
flip-frontend a114bcfa62bb14bb73420fd67da81d0352022b84320e95cc73c0abb1082a664f
```

## Recovery acceptance

Hardware testing proved all of the following on the real Miyoo Flip:

- Save Safety reports structural health for current, previous and recovery cartridge saves and resume availability.
- `Start Normally` loads cartridge SRAM but does not restore the current emulator state.
- Two distinct Pokemon Yellow cartridge-save generations were created:
  - newer current: `98e0aef6b838b800ec323f98657690ba9ec1cb390f34d49b204c0d746480921f`
  - older protected previous: `70c90f16530b74668488825d9eb8b3fe43293f2aa66c42a4102e715ecc889048`
- `Restore Previous Save` activated the older cartridge save, preserved the newer cartridge save as `.sav.recovery`, preserved the matching state as `.state.recovery`, removed current `.state` from automatic resume, created `.restore-pending`, and reported `resume=BLOCKED`.
- Pokemon reached its normal title flow and its own CONTINUE command visibly loaded the selected older progress.
- The restore marker remained present while the restored timeline was running.
- A controlled exit committed both a fresh `.sav` and fresh `.state`; only then was `.restore-pending` cleared and Resume Game re-enabled.
- `Undo Last Restore` restored the newer pre-restore cartridge save and matching recovery state; a visual Resume Game check returned to the newer progress.
- With no current state, a new restore generation removed a stale older `.state.recovery` instead of pairing it with the newer `.sav.recovery` generation.
- Save-only Undo with no matching recovery state restored cartridge-save data while keeping resume blocked and leaving both current and recovery state unavailable.
- The deliberately destructive test case that removed a current state while a resumed emulator instance was still running caused `FLIP_STATE: SAVE_FAILED stage=preserve-previous`; FlipOS failed closed and kept automatic resume blocked. This was test interference, not a product defect.

## Lifecycle and regression acceptance

Regression testing on the same candidate proved:

- normal Resume Game restores current state successfully;
- lid close commits both `.sav` and `.state` before suspend;
- lid wake restores the game correctly;
- START+SELECT controlled exit commits both `.sav` and `.state` and exits cleanly;
- RK817 deep-resume detection still matches the exact confirmed broken signature `17=06 38=00 40=69`;
- exact-signature RK817 recovery was applied and audio recovered successfully;
- Gambatte source audio remains 32768 Hz and RK817 output remains 32000 Hz;
- observed frame timing remained around the established 59.728 FPS target, with typical one-second measurements around 59.95-59.99 FPS and occasional 59-frame windows;
- user-observed resume, lid resume, audio after resume and audio sync all passed.

## Automatic lid-resume regression

The first Phase 5C frontend candidate inserted the Game Options screen into the existing automatic post-lid relaunch. Root cause was that the Save Safety wrapper could not distinguish a deliberate game launch from the frontend's internal automatic wake relaunch.

A TDD regression fix added explicit `automatic_resume` intent to session metadata. The flag is set only for the one post-lid relaunch and causes the Save Safety wrapper to bypass Game Options only in that case. Deliberate launches still expose Resume Game, Start Normally and Save Safety.

Hardware proof after the fix:

```text
FRONTEND_STANDBY auto resume rom=/mnt/SDCARD/Roms/GB/Pokemon - Yellow.gb
FLIP_SAVE_SAFETY: AUTOMATIC_RESUME
flip_audio: RK817 deep-resume recovery required
flip_audio: RK817 deep-resume recovery applied
flip_audio: RK817 audio recovered
FLIP_STATE: RESTORE_CURRENT_OK path=/mnt/SDCARD/flipos-dev/states/gb/792f9628ba43b7d9.state size=84226
```

The user confirmed that opening the lid resumed Pokemon directly with no Game Options screen.

## Result

Phase 5C Save Safety / Recovery hardware acceptance is complete for the scoped single-generation recovery design. Multi-generation rotating backups remain intentionally deferred to a later phase.
