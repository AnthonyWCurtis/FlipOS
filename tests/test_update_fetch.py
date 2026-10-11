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
    archive = serve / "FlipOSUpdate.tar.gz"
    with tarfile.open(archive, "w:gz") as tf:
        tf.add(package, arcname="FlipOSUpdate")

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
                            "browser_download_url": f"http://127.0.0.1:{port}/FlipOSUpdate.tar.gz.sha256"
                        },
                        {
                            "browser_download_url": f"http://127.0.0.1:{port}/FlipOSUpdate.tar.gz"
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
    finally:
        server.shutdown()
        server.server_close()
        os.chdir(old)

print("update fetch tests passed")
