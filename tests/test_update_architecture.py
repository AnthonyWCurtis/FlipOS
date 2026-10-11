from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
installer = (ROOT / "tools/update/flip-update-install").read_text()
deploy = (ROOT / "tools/device/deploy-playtest").read_text()

for token in (
    'miyoo-flip-rk3566',
    'sha256sum -c',
    'STAGED UPDATE CHECKSUM FAILED',
    'UPDATE CONTAINS DISALLOWED FILE',
    'ROLLBACK=',
    'rollback_install()',
    'installed.list',
    'last-good',
    'UPDATE COMPLETE - RESTART TO FINISH',
):
    assert token in installer, token

assert 'state/settings' not in installer.split('case "$rel" in', 1)[1].split('esac', 1)[0]
assert 'flip-update-install' in deploy

print("update architecture tests passed")

frontend = (ROOT / "src/frontend/flip_frontend.c").read_text()
assert 'SCREEN_UPDATE' in frontend
assert 'run_update_installer()' in frontend
assert 'UPDATE FAILED - CURRENT VERSION KEPT' in frontend
fetcher = (ROOT / "tools/update/flip-update-fetch").read_text()
assert 'api.github.com/repos/AnthonyWCurtis/FlipOS/releases?per_page=10' in fetcher
assert 'FlipOSUpdate.tar.gz' in fetcher
assert 'browser_download_url' in fetcher
assert 'sha256sum -c' in fetcher
assert 'run_update_fetcher()' in frontend

assert 'FLIPOS RELEASES ARE NOT PUBLIC YET' in fetcher
assert 'grep "/$ASSET_NAME$"' in fetcher
assert 'gzip -dc "$ARCHIVE" | tar -xf -' in fetcher
assert "tr ',' '\\n' < \"$RELEASES\"" in fetcher
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
