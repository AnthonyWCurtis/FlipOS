#!/usr/bin/env python3
from pathlib import Path
import hashlib
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
INSTALLER = ROOT / "tools/update/flip-update-install"


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def make_package(base: Path, files: dict[str, bytes]) -> Path:
    package = base / "package"
    payload = package / "payload"
    payload.mkdir(parents=True)
    (package / "manifest").write_text(
        "format=1\n"
        "target=miyoo-flip-rk3566\n"
        "version=0.1-test\n"
        "build_id=test-build\n"
    )
    lines = []
    for rel, data in files.items():
        path = payload / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        lines.append(f"{digest(path)}  {rel}\n")
    (package / "SHA256SUMS").write_text("".join(lines))
    return package


def run(
    installer_root: Path,
    package: Path | None = None,
    *,
    boot_updater: Path | None = None,
    fault: str | None = None,
    recover: bool = False,
):
    env = os.environ.copy()
    env["FLIP_UPDATE_ROOT"] = str(installer_root)
    env["FLIP_UPDATE_PROGRESS"] = str(installer_root / "progress")
    if boot_updater is not None:
        env["FLIP_UPDATE_BOOT_UPDATER"] = str(boot_updater)
    if fault is not None:
        env["FLIP_UPDATE_TEST_FAULT"] = fault

    args = [str(INSTALLER)]
    if recover:
        args.append("--recover-interrupted")
    else:
        assert package is not None
        args.append(str(package))

    return subprocess.run(args, env=env, text=True, capture_output=True)


def seed_install(base: Path):
    install = base / "install"
    (install / "bin").mkdir(parents=True)
    (install / "bin/flip-frontend").write_bytes(b"old-frontend")
    (install / "VERSION").write_bytes(b"old-version\n")
    (install / "state/settings").mkdir(parents=True)
    (install / "state/settings/global").write_text("volume=45\n")
    boot = base / "boot/updater"
    boot.parent.mkdir(parents=True)
    boot.write_bytes(b"old-boot-hook\n")
    return install, boot


def update_files():
    return {
        "bin/flip-frontend": b"new-frontend",
        "VERSION": b"0.1-test\n",
        "updater-playtest": b"new-boot-hook\n",
    }


def assert_old_version(install: Path, boot: Path):
    assert (install / "bin/flip-frontend").read_bytes() == b"old-frontend"
    assert (install / "VERSION").read_bytes() == b"old-version\n"
    assert boot.read_bytes() == b"old-boot-hook\n"
    assert (install / "state/settings/global").read_text() == "volume=45\n"


def assert_new_version(install: Path, boot: Path):
    assert (install / "bin/flip-frontend").read_bytes() == b"new-frontend"
    assert (install / "VERSION").read_bytes() == b"0.1-test\n"
    assert boot.read_bytes() == b"new-boot-hook\n"
    assert (install / "state/settings/global").read_text() == "volume=45\n"


# Normal transaction: install and commit, preserving user state and rollback data.
with tempfile.TemporaryDirectory(prefix="flipos-update-") as tmp:
    base = Path(tmp)
    install, boot = seed_install(base)
    package = make_package(base, update_files())
    result = run(install, package, boot_updater=boot)
    assert result.returncode == 0, result.stderr
    assert_new_version(install, boot)
    assert "phase=complete" in (install / "progress").read_text()
    assert (install / "state/update/last-good").exists()
    assert not (install / "state/update/lock").exists()
    rollback = install / ".update-rollback/current"
    assert (rollback / "files/bin/flip-frontend").read_bytes() == b"old-frontend"
    assert (rollback / "committed").exists()
    assert (rollback / "boot-updater").read_bytes() == b"old-boot-hook\n"

# Package corruption is rejected before live mutation.
with tempfile.TemporaryDirectory(prefix="flipos-update-bad-") as tmp:
    base = Path(tmp)
    install, boot = seed_install(base)
    package = make_package(base, {"bin/flip-frontend": b"new"})
    (package / "payload/bin/flip-frontend").write_bytes(b"tampered")
    result = run(install, package, boot_updater=boot)
    assert result.returncode != 0
    assert (install / "bin/flip-frontend").read_bytes() == b"old-frontend"
    assert "phase=failed" in (install / "progress").read_text()
    assert not (install / "state/update/lock").exists()

# User-state paths remain outside the update allowlist.
with tempfile.TemporaryDirectory(prefix="flipos-update-path-") as tmp:
    base = Path(tmp)
    install = base / "install"
    package = make_package(base, {"state/settings/global": b"bad"})
    result = run(install, package)
    assert result.returncode != 0
    assert not (install / "state/settings/global").exists()

# An unknown/legacy lock has no proven transaction semantics. Recovery must
# preserve it and fail closed so the boot hook can fall back to the stock OS.
with tempfile.TemporaryDirectory(prefix="flipos-update-unknown-lock-") as tmp:
    base = Path(tmp)
    install = base / "install"
    lock = install / "state/update/lock"
    lock.mkdir(parents=True)
    recovered = run(install, recover=True)
    assert recovered.returncode != 0
    assert lock.is_dir()
    assert "FLIP_UPDATE_RECOVERY_UNKNOWN_TRANSACTION" in recovered.stderr

# Power loss after the write-ahead journal but before the first rename must be
# recoverable even though no live file has changed yet.
with tempfile.TemporaryDirectory(prefix="flipos-update-journal-loss-") as tmp:
    base = Path(tmp)
    install, boot = seed_install(base)
    package = make_package(base, update_files())
    interrupted = run(
        install, package, boot_updater=boot, fault="after-first-journal"
    )
    assert interrupted.returncode != 0
    assert (install / "state/update/lock").is_dir()
    recovered = run(install, boot_updater=boot, recover=True)
    assert recovered.returncode == 0, recovered.stderr
    assert "FLIP_UPDATE_RECOVERY_OK" in recovered.stdout
    assert_old_version(install, boot)
    assert not (install / "state/update/lock").exists()

# Power loss immediately after rollback preparation has no live mutations, but
# the durable lock still drives deterministic recovery.
with tempfile.TemporaryDirectory(prefix="flipos-update-prepared-loss-") as tmp:
    base = Path(tmp)
    install, boot = seed_install(base)
    package = make_package(base, update_files())
    interrupted = run(
        install, package, boot_updater=boot, fault="after-prepare"
    )
    assert interrupted.returncode != 0
    assert (install / ".update-rollback/current/prepared").exists()
    recovered = run(install, boot_updater=boot, recover=True)
    assert recovered.returncode == 0, recovered.stderr
    assert_old_version(install, boot)

# Power loss immediately after one live replacement must restore that file.
with tempfile.TemporaryDirectory(prefix="flipos-update-first-file-loss-") as tmp:
    base = Path(tmp)
    install, boot = seed_install(base)
    package = make_package(base, update_files())
    interrupted = run(
        install, package, boot_updater=boot, fault="after-first-install"
    )
    assert interrupted.returncode != 0
    assert (install / "bin/flip-frontend").read_bytes() == b"new-frontend"
    recovered = run(install, boot_updater=boot, recover=True)
    assert recovered.returncode == 0, recovered.stderr
    assert_old_version(install, boot)
    assert (install / ".update-rollback/current/recovered").exists()

# Power loss after the boot-hook write-ahead marker but before its rename must
# restore all runtime files and retain the previous boot hook.
with tempfile.TemporaryDirectory(prefix="flipos-update-boot-journal-loss-") as tmp:
    base = Path(tmp)
    install, boot = seed_install(base)
    package = make_package(base, update_files())
    interrupted = run(
        install, package, boot_updater=boot, fault="after-boot-journal"
    )
    assert interrupted.returncode != 0
    assert boot.read_bytes() == b"old-boot-hook\n"
    recovered = run(install, boot_updater=boot, recover=True)
    assert recovered.returncode == 0, recovered.stderr
    assert_old_version(install, boot)

# Power loss after all runtime files but before commit must roll the complete
# transaction back, including the separately installed boot hook.
with tempfile.TemporaryDirectory(prefix="flipos-update-precommit-loss-") as tmp:
    base = Path(tmp)
    install, boot = seed_install(base)
    package = make_package(base, update_files())
    interrupted = run(
        install, package, boot_updater=boot, fault="before-commit"
    )
    assert interrupted.returncode != 0
    assert_new_version(install, boot)
    recovered = run(install, boot_updater=boot, recover=True)
    assert recovered.returncode == 0, recovered.stderr
    assert_old_version(install, boot)

# Power loss after the durable commit marker must keep the new version. Boot
# recovery only clears the stale lock and finalizes the completed transaction.
with tempfile.TemporaryDirectory(prefix="flipos-update-postcommit-loss-") as tmp:
    base = Path(tmp)
    install, boot = seed_install(base)
    package = make_package(base, update_files())
    interrupted = run(
        install, package, boot_updater=boot, fault="after-commit"
    )
    assert interrupted.returncode != 0
    assert (install / "state/update/lock").is_dir()
    assert (install / ".update-rollback/current/committed").exists()
    recovered = run(install, boot_updater=boot, recover=True)
    assert recovered.returncode == 0, recovered.stderr
    assert "FLIP_UPDATE_RECOVERY_FINALIZED_COMMIT" in recovered.stdout
    assert_new_version(install, boot)
    assert not (install / "state/update/lock").exists()

print("update installer tests passed")
