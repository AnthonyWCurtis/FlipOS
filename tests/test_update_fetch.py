#!/usr/bin/env python3
from pathlib import Path
import hashlib
import http.server
import json
import os
import shutil
import socketserver
import subprocess
import tarfile
import tempfile
import threading

ROOT = Path(__file__).resolve().parents[1]
FETCH = ROOT / "tools/update/flip-update-fetch"

with tempfile.TemporaryDirectory(prefix="flipos-fetch-") as td:
    base = Path(td)
    serve = base / "serve"
    package = base / "make/FlipOSUpdate"
    payload = package / "payload"
    payload.mkdir(parents=True)
    (payload / "VERSION").write_text("beta-test\n")
    digest = hashlib.sha256((payload / "VERSION").read_bytes()).hexdigest()
    (package / "SHA256SUMS").write_text(f"{digest}  VERSION\n")
    (package / "manifest").write_text(
        "format=1\ntarget=miyoo-flip-rk3566\nversion=beta-test\nbuild_id=test\n"
    )
    serve.mkdir()
    archive = serve / "FlipOS-Update.tar.gz"
    # Production release uses ordinary short-name TAR headers, not Python's
    # default PAX timestamp-extension records.
    with tarfile.open(archive, "w:gz", format=tarfile.USTAR_FORMAT) as tf:
        tf.add(package, arcname="FlipOSUpdate")
    sidecar = serve / "FlipOS-Update.tar.gz.sha256"

    def write_sidecar():
        sidecar.write_text(
            f"{hashlib.sha256(archive.read_bytes()).hexdigest()}  {archive.name}\n"
        )

    write_sidecar()

    class Handler(http.server.SimpleHTTPRequestHandler):
        def log_message(self, *args):
            pass

        def do_GET(self):
            if self.path.startswith("/api"):
                body = json.dumps([{
                    "tag_name": "v0.1.0-beta.1",
                    "prerelease": True,
                    "assets": [
                        {
                            "browser_download_url": f"http://127.0.0.1:{port}/FlipOS-Update.tar.gz.sha256"
                        },
                        {
                            "browser_download_url": f"http://127.0.0.1:{port}/FlipOS-Update.tar.gz"
                        },
                    ]
                }]).encode()
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)
                return
            return super().do_GET()

    old = os.getcwd()
    os.chdir(serve)
    server = socketserver.TCPServer(("127.0.0.1", 0), Handler)
    port = server.server_address[1]
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        install = base / "install"
        dest = base / "downloaded/FlipOSUpdate"
        env = os.environ.copy()
        env["FLIP_UPDATE_ROOT"] = str(install)
        env["FLIP_UPDATE_DEST"] = str(dest)
        env["FLIP_UPDATE_PROGRESS"] = str(base / "progress")
        env["FLIP_UPDATE_API_URL"] = f"http://127.0.0.1:{port}/api"
        result = subprocess.run([str(FETCH)], env=env, text=True, capture_output=True)
        assert result.returncode == 0, result.stderr
        assert (dest / "manifest").exists()
        assert (dest / "payload/VERSION").read_text() == "beta-test\n"
        assert "phase=ready" in (base / "progress").read_text()
        assert "FLIP_UPDATE_FETCH_OK" in result.stdout

        # Background checks must fetch release metadata only: no archive,
        # extraction or staged install package is created.
        shutil.rmtree(dest, ignore_errors=True)
        install.mkdir(parents=True, exist_ok=True)
        (install / "VERSION").write_text("0.1.0-beta.0\n")
        check_only = subprocess.run(
            [str(FETCH), "--check-only"],
            env=env,
            text=True,
            capture_output=True,
        )
        assert check_only.returncode == 0, check_only.stderr
        assert "FLIP_UPDATE_CHECK_AVAILABLE" in check_only.stdout
        assert not dest.exists()
        online_available = install / "state/update/online-available"
        assert online_available.read_text().strip() == "0.1.0-beta.1"

        # A metadata-only check also clears its online marker when current,
        # without mutating the downloaded-package marker.
        (install / "VERSION").write_text("0.1.0-beta.1\n")
        available = install / "state/update/available"
        available.parent.mkdir(parents=True, exist_ok=True)
        available.write_text("staged-package\n")
        latest_check = subprocess.run(
            [str(FETCH), "--check-only"],
            env=env,
            text=True,
            capture_output=True,
        )
        assert latest_check.returncode == 0, latest_check.stderr
        assert "FLIP_UPDATE_FETCH_LATEST" in latest_check.stdout
        assert not online_available.exists()
        assert available.exists()
        assert not dest.exists()

        # When the installed version already matches the latest release tag,
        # report success without presenting the staged package as an update.
        install.mkdir(parents=True, exist_ok=True)
        (install / "VERSION").write_text("0.1.0-beta.1\n")
        available.write_text("stale\n")
        latest = subprocess.run(
            [str(FETCH)], env=env, text=True, capture_output=True
        )
        assert latest.returncode == 0, latest.stderr
        assert "FLIP_UPDATE_FETCH_LATEST" in latest.stdout
        assert not available.exists()
        assert "YOU'RE ON THE LATEST RELEASE" in (base / "progress").read_text()

        # Full downloads must reject altered bytes, even when the internal
        # tar member SHA256SUMS would otherwise look self-consistent.
        install.joinpath("VERSION").write_text("0.1.0-beta.0\n")
        original = archive.read_bytes()
        dest.parent.mkdir(parents=True, exist_ok=True)
        if dest.exists():
            shutil.rmtree(dest)
        (dest / "sentinel").parent.mkdir(parents=True, exist_ok=True)
        (dest / "sentinel").write_text("prior-staged-update\n")

        def expect_rejected(label, expected_error):
            result = subprocess.run(
                [str(FETCH)], env=env, text=True, capture_output=True
            )
            assert result.returncode != 0, (label, result.stdout, result.stderr)
            assert expected_error in result.stderr, (label, result.stderr)
            assert (dest / "sentinel").read_text() == "prior-staged-update\n", (
                label, "rejected archive altered previously staged update"
            )
            assert not (base / "escaped").exists(), label
            assert not (base / "outside").exists(), label

        archive.write_bytes(original + b"tampered")
        expect_rejected("mismatched-sidecar", "UPDATE ARCHIVE CHECKSUM FAILED")
        archive.write_bytes(original)
        sidecar.write_text("invalid checksum sidecar\n")
        expect_rejected("invalid-sidecar", "INVALID UPDATE CHECKSUM FILE")
        write_sidecar()

        def make_bad_tar(label, entries):
            with tarfile.open(archive, "w:gz", format=tarfile.USTAR_FORMAT) as tf:
                for name, kind, link in entries:
                    item = tarfile.TarInfo(name)
                    if kind == "dir":
                        item.type = tarfile.DIRTYPE
                    elif kind == "symlink":
                        item.type = tarfile.SYMTYPE
                        item.linkname = link
                    elif kind == "hardlink":
                        item.type = tarfile.LNKTYPE
                        item.linkname = link
                    else:
                        item.size = 0
                    tf.addfile(item)
            write_sidecar()
            expect_rejected(label, "UNSAFE UPDATE ARCHIVE")

        for label, entries in (
            ("parent-traversal", [
                ("FlipOSUpdate/../escaped", "file", ""),
            ]),
            ("absolute-path", [
                ("/outside", "file", ""),
            ]),
            ("symlink", [
                ("FlipOSUpdate/payload/bin", "symlink", "../../../outside"),
            ]),
            ("hardlink", [
                ("FlipOSUpdate/payload/bin/flip-frontend", "hardlink", "/outside"),
            ]),
            ("device-entry", [
                ("FlipOSUpdate/payload/bin/device", "file", ""),
                ("FlipOSUpdate/payload/bin/char-device", "device", ""),
            ]),
            ("unapproved-content", [
                ("FlipOSUpdate/payload/malicious.sh", "file", ""),
            ]),
            ("duplicate-entry", [
                ("FlipOSUpdate/manifest", "file", ""),
                ("FlipOSUpdate/manifest", "file", ""),
            ]),
        ):
            if label == "device-entry":
                with tarfile.open(archive, "w:gz", format=tarfile.USTAR_FORMAT) as tf:
                    item = tarfile.TarInfo("FlipOSUpdate/payload/bin/device")
                    item.type = tarfile.CHRTYPE
                    item.devmajor = 1
                    item.devminor = 3
                    tf.addfile(item)
                write_sidecar()
                expect_rejected(label, "UNSAFE UPDATE ARCHIVE")
            else:
                make_bad_tar(label, entries)

        # A matching sidecar must not permit malformed TAR headers.
        import gzip
        with gzip.open(archive, "wb") as zipped:
            zipped.write(b"not-a-tar-header" + b"\x00" * 1024)
        write_sidecar()
        result = subprocess.run([str(FETCH)], env=env, text=True, capture_output=True)
        assert result.returncode != 0
        assert "INVALID UPDATE TAR ALIGNMENT" in result.stderr or (
            "UNSAFE UPDATE ARCHIVE" in result.stderr
        )

        archive.write_bytes(original)
        write_sidecar()
    finally:
        server.shutdown()
        server.server_close()
        os.chdir(old)

print("update fetch tests passed")
