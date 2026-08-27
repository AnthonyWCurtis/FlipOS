#!/usr/bin/env python3
from pathlib import Path

path = Path("docs/ROADMAP.md")
text = path.read_text()
old = """### Phase 5C — Recovery Tooling / Policy

- [ ] Save Safety / Recovery design for user-facing tooling
- [ ] Rotating backups if justified by tested policy
- [ ] Recovery UI/tool
- [ ] Manual restore workflow
"""
new = """### Phase 5C — Recovery Tooling / Policy

- [x] Save Safety / Recovery design for user-facing tooling
- [x] Core-aware Save Safety status and management commands
- [x] Generic frontend Resume Game / Start Normally / Save Safety actions
- [x] Non-destructive Restore Previous Save workflow
- [x] Paired pre-restore `.sav.recovery` / `.state.recovery` generation
- [x] Persistent `.restore-pending` resume block across restart/reboot
- [x] Fresh-state re-enable only after cartridge save + state both commit successfully
- [x] Undo Last Restore with paired-state and save-only safety behavior
- [x] Stale recovery-state generation rejection
- [x] Automatic lid-wake resume bypasses Game Options while deliberate launches remain interactive
- [x] Full native regression suite and ARM64 static build pass
- [x] Real-Miyoo-Flip hardware validation passes
- [ ] Multi-generation rotating backups, only if justified by later policy/testing

Hardware-tested code commit: `593eb53e54334fae67bd377b6f02bab107436ac2` (`Restore automatic lid resume through Save Safety`). Final tested binary SHA-256 values were `537eb2518739aa7ae947e56ce7aef5805845a58eda8d995373c2f48b194337cd` for `flip-gb` and `a114bcfa62bb14bb73420fd67da81d0352022b84320e95cc73c0abb1082a664f` for `flip-frontend`.

Hardware acceptance used Pokemon Yellow with distinct current/previous cartridge-save generations. Restore Previous activated the older progress, preserved the newer timeline as recovery data, quarantined the old resume state and kept Resume blocked until a fresh matching state was committed. Visual CONTINUE confirmed older restored progress; paired Undo visually returned to the newer progress. A no-current-state generation proved stale `.state.recovery` removal, and save-only Undo kept resume blocked when no matching recovery state existed. The recovery marker failed closed under deliberately destructive test interference.

Lifecycle regression verified Resume Game, lid suspend/resume, START+SELECT persistence, exact RK817 deep-resume recovery, Gambatte 32768 Hz -> RK817 32000 Hz audio conversion and established ~59.7 FPS timing. A Phase 5C wake regression that initially showed Game Options after lid-open was fixed with explicit one-shot automatic-resume session intent; hardware logs then showed `FRONTEND_STANDBY auto resume`, `FLIP_SAVE_SAFETY: AUTOMATIC_RESUME`, RK817 recovery and `RESTORE_CURRENT_OK`, while the user observed direct game resume with no menu.

Detailed evidence: `docs/superpowers/specs/2026-08-27-phase5c-hardware-acceptance.md`.
"""
count = text.count(old)
if count != 1:
    raise SystemExit(f"ROADMAP anchor count={count}; expected 1")
path.write_text(text.replace(old, new, 1))
Path(__file__).unlink()
print("Phase 5C roadmap closeout applied")
