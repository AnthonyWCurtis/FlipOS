#!/usr/bin/env python3
from pathlib import Path
import hashlib
import os
import shutil
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


def run(installer_root: Path, package: Path):
    env = os.environ.copy()
    env["FLIP_UPDATE_ROOT"] = str(installer_root)
    env["FLIP_UPDATE_PROGRESS"] = str(installer_root / "progress")
    return subprocess.run(
        [str(INSTALLER), str(package)],
        env=env,
        text=True,
        capture_output=True,
    )


with tempfile.TemporaryDirectory(prefix="flipos-update-") as tmp:
    base = Path(tmp)
    install = base / "install"
    (install / "bin").mkdir(parents=True)
    (install / "bin/flip-frontend").write_bytes(b"old-frontend")
    (install / "state/settings").mkdir(parents=True)
    (install / "state/settings/global").write_text("volume=45\n")
    update_state = install / "state/update"
    update_state.mkdir(parents=True)
    (update_state / "available").write_text("0.1-test\n")
    (update_state / "online-available").write_text("0.1-test\n")

    package = make_package(base, {
        "bin/flip-frontend": b"new-frontend",
        "VERSION": b"0.1-test\n",
    })
    result = run(install, package)
    assert result.returncode == 0, result.stderr
    assert (install / "bin/flip-frontend").read_bytes() == b"new-frontend"
    assert (install / "state/settings/global").read_text() == "volume=45\n"
    assert "phase=complete" in (install / "progress").read_text()
    assert (install / "state/update/last-good").exists()
    assert not (install / "state/update/available").exists()
    assert not (install / "state/update/online-available").exists()
    assert (install / ".update-rollback/current/files/bin/flip-frontend").read_bytes() == b"old-frontend"

with tempfile.TemporaryDirectory(prefix="flipos-update-bad-") as tmp:
    base = Path(tmp)
    install = base / "install"
    (install / "bin").mkdir(parents=True)
    (install / "bin/flip-frontend").write_bytes(b"old")
    package = make_package(base, {"bin/flip-frontend": b"new"})
    (package / "payload/bin/flip-frontend").write_bytes(b"tampered")
    result = run(install, package)
    assert result.returncode != 0
    assert (install / "bin/flip-frontend").read_bytes() == b"old"
    assert "phase=failed" in (install / "progress").read_text()

with tempfile.TemporaryDirectory(prefix="flipos-update-path-") as tmp:
    base = Path(tmp)
    install = base / "install"
    package = make_package(base, {"state/settings/global": b"bad"})
    result = run(install, package)
    assert result.returncode != 0
    assert not (install / "state/settings/global").exists()

print("update installer tests passed")
