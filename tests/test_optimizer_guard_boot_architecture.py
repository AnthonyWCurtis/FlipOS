#!/usr/bin/env python3

from pathlib import Path

wrapper = Path("tools/device/updater-playtest").read_text()

errors = []

required = [
    'SUPERVISOR="$ROOT/bin/flip-optimizer-supervisor"',
    '"$SUPERVISOR" boot-enforce',
    'RAM_STOCK_SAFE=/tmp/flipos-optimizer-stock-safe',
    'SD_WRITABLE=0',
    '/proc/mounts',
    'LOG_TARGET="$RAM_LOG"',
    'SD read-only; durable settlement deferred',
    '"$ROOT/bin/flip-optimizer-guard" boot-check',
    '"$ROOT/bin/flip-optimizer-calibration" recover-boot',
    'post-settlement enforcement',
    '"$ROOT/bin/flip-optimizer-session" boot-resume',
    'runtime stock-safe remains authoritative',
    'if [ "$SD_WRITABLE" -eq 1 ] &&',
    '"$ROOT/bin/flip-frontend" >> "$LOG_TARGET" 2>&1',
]

for token in required:
    if token not in wrapper:
        errors.append(f"optimizer recovery boot wrapper missing: {token}")

supervisor_pos = wrapper.find('"$SUPERVISOR" boot-enforce')
first_sd_write_candidates = [
    pos for pos in (
        wrapper.find('mkdir -p "$ROOT/logs"'),
        wrapper.find(': > "$LOG_TARGET"'),
        wrapper.find('chmod 755 "$ROOT/bin/flip-frontend"'),
    )
    if pos >= 0
]
first_sd_write = min(first_sd_write_candidates) if first_sd_write_candidates else -1
guard_pos = wrapper.find('"$ROOT/bin/flip-optimizer-guard" boot-check')
reconcile_pos = wrapper.find('"$ROOT/bin/flip-optimizer-calibration" recover-boot')
post_settlement_pos = wrapper.find('post-settlement enforcement')
session_pos = wrapper.find('"$ROOT/bin/flip-optimizer-session" boot-resume')
logger_pos = wrapper.find('\n    "$ROOT/bin/flip-playtest-logger.sh"\n')
frontend_pos = wrapper.find('"$ROOT/bin/flip-frontend" >> "$LOG_TARGET" 2>&1')

if min(
    supervisor_pos,
    first_sd_write,
    guard_pos,
    reconcile_pos,
    post_settlement_pos,
    session_pos,
    logger_pos,
    frontend_pos,
) < 0:
    pass
elif not (
    supervisor_pos
    < first_sd_write
    < guard_pos
    < reconcile_pos
    < post_settlement_pos
    < session_pos
    < logger_pos
    < frontend_pos
):
    errors.append(
        "boot safety order must be supervisor, optional writes, guard settlement, "
        "calibration reconcile, post-settlement enforcement, optimizer session, "
        "logger, frontend"
    )

supervisor_line = next(
    (line for line in wrapper.splitlines() if '"$SUPERVISOR" boot-enforce' in line),
    "",
)
if '>> "$LOG"' in supervisor_line or '>> "$LOG_TARGET"' in supervisor_line:
    errors.append("supervisor boot enforcement must not depend on SD logging")

if errors:
    for error in errors:
        print(error)
    raise SystemExit(1)

print("optimizer supervisor boot architecture passed")
