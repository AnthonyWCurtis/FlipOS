#!/usr/bin/env python3
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
BUILDER = ROOT / "tools/update/build-release-package"

required = [
    "build/flip-frontend",
    "build/flip-gb",
    "build/flip-gba",
    "build/flip-gpu-service",
    "build/flip-charge-service",
    "build/umtprd",
    "build/flip-optimizer-runtime",
    "build/flip_opp_voltage.ko",
    "build/flip_opp_runtime_profile.ko",
    "build/flip_tsadc_recovery.ko",
    "build/flip_rk817_charger.ko",
]
for rel in required:
    assert (ROOT / rel).exists(), rel

with tempfile.TemporaryDirectory(prefix="flipos-release-") as td:
    out = Path(td)
    result = subprocess.run(
        [str(BUILDER), "0.1.0-beta.1", str(out)],
        cwd=ROOT,
        text=True,
        capture_output=True,
    )
    assert result.returncode == 0, result.stderr
    archive = out / "FlipOS-Update.tar.gz"
    assert archive.exists()
    assert (out / "FlipOS-Update.tar.gz.sha256").exists()
    with tarfile.open(archive, "r:gz") as tf:
        names = set(tf.getnames())
        assert "FlipOSUpdate/manifest" in names
        assert "FlipOSUpdate/SHA256SUMS" in names
        assert "FlipOSUpdate/payload/bin/flip-update-fetch" in names
        assert "FlipOSUpdate/payload/bin/flip-update-install" in names
        assert "FlipOSUpdate/payload/bin/umtprd" in names
        assert "FlipOSUpdate/payload/bin/flip-gpu-service" in names
        assert "FlipOSUpdate/payload/bin/flip-usb-mtp" in names
        assert "FlipOSUpdate/payload/etc/umtprd.conf" in names
        assert "FlipOSUpdate/payload/licenses/uMTP-Responder-GPL-3.0.txt" in names
        assert "FlipOSUpdate/payload/licenses/uMTP-Responder-UPSTREAM.txt" in names
        assert "FlipOSUpdate/payload/licenses/THIRD_PARTY_NOTICES.md" in names
        manifest = tf.extractfile("FlipOSUpdate/manifest").read().decode()
        assert "version=0.1.0-beta.1" in manifest
        assert "target=miyoo-flip-rk3566" in manifest

    fresh_builder = ROOT / "tools/update/build-beta-install-bundle"
    fresh = subprocess.run(
        [str(fresh_builder), "0.1.0-beta.1", str(out)],
        cwd=ROOT,
        text=True,
        capture_output=True,
    )
    assert fresh.returncode == 0, fresh.stderr
    install_zip = out / "FlipOS-Fresh_Install.zip"
    assert install_zip.exists()
    with zipfile.ZipFile(install_zip, "r") as zf:
        names = set(zf.namelist())
        assert "flipos-dev/bin/flip-gpu-service" in names
        assert ".tmp_update/updater" in names

print("release package tests passed")
