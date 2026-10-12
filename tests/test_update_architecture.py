from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
installer = (ROOT / "tools/update/flip-update-install").read_text()
deploy = (ROOT / "tools/device/deploy-playtest").read_text()
boot_hook = (ROOT / "tools/device/updater-playtest").read_text()

for token in (
    'miyoo-flip-rk3566',
    'sha256sum -c',
    'STAGED UPDATE CHECKSUM FAILED',
    'UPDATE CONTAINS DISALLOWED FILE',
    'ROLLBACK=',
    'rollback_install()',
    'installed.list',
    'files.list',
    'prepared',
    'committed',
    '--recover-interrupted',
    'cat > "$LOCK/transaction"',
    'FLIP_UPDATE_RECOVERY_UNKNOWN_TRANSACTION',
    'TRANSACTION_DIRTY=1',
    'fault_inject after-prepare',
    'fault_inject after-first-journal',
    'fault_inject after-first-install',
    'fault_inject after-boot-journal',
    'fault_inject before-commit',
    'fault_inject after-commit',
    'last-good',
    'UPDATE COMPLETE - RESTART TO FINISH',
):
    assert token in installer, token

assert 'state/settings' not in installer.split('case "$rel" in', 1)[1].split('esac', 1)[0]
assert 'flip-update-install' in deploy

for token in (
    'UPDATE_LOCK="$ROOT/state/update/lock"',
    '"$UPDATE_INSTALLER" --recover-interrupted',
    'UPDATE_RECOVERY_LOG=/tmp/flipos-update-recovery.log',
    'exec /usr/miyoo/bin/runmiyoo-original.sh',
):
    assert token in boot_hook, token

frontend = (ROOT / "src/frontend/flip_frontend.c").read_text()
assert 'SCREEN_UPDATE' in frontend
assert 'run_update_installer()' in frontend
assert 'FLIP_UPDATE_LOCK' in frontend
assert 'UPDATE INTERRUPTED - RESTART TO RECOVER' in frontend
assert 'UPDATE FAILED - CURRENT VERSION KEPT' in frontend
fetcher = (ROOT / "tools/update/flip-update-fetch").read_text()
assert 'api.github.com/repos/AnthonyWCurtis/FlipOS/releases?per_page=10' in fetcher
assert 'FlipOS-Update.tar.gz' in fetcher
assert 'browser_download_url' in fetcher
assert 'sha256sum -c' in fetcher
assert 'run_update_fetcher()' in frontend

assert 'FLIPOS RELEASES ARE NOT PUBLIC YET' in fetcher
assert 'FIELDS="$WORK/releases.fields"' in fetcher
assert 'while IFS= read -r field || [ -n "$field" ]; do' in fetcher
assert '*/"$ASSET_NAME")' in fetcher
assert '[ -n "$TAG" ] || fail "UPDATE RELEASE TAG MISSING"' in fetcher
assert 'gzip -dc "$ARCHIVE" | tar -xf -' in fetcher
assert "tr ',' '\\n' < \"$RELEASES\" > \"$FIELDS\"" in fetcher
assert 'trap cleanup EXIT' not in fetcher
assert "trap 'cleanup; exit 1' HUP INT TERM" in fetcher
assert 'AVAILABLE="$STATE/available"' in fetcher
assert 'INSTALLED_VERSION="$ROOT/VERSION"' in fetcher
assert 'rm -f "$AVAILABLE"' in installer


assert 'delete_update_package()' in frontend
assert '"A INSTALL   X CHECK GITHUB   Y DELETE"' in frontend
assert '"DOWNLOADED UPDATE DELETED"' in frontend
assert 'unlink(FLIP_UPDATE_AVAILABLE)' in frontend

assert '"A RESTART NOW   B LATER"' in frontend
assert 'if (update_complete)' in frontend
assert 'reboot(RB_AUTOBOOT)' in frontend
