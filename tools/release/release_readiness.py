#!/usr/bin/env python3
"""Fail-closed review of a FlipOS release candidate's evidence manifest.

This verifies record consistency and referenced local file digests, NOT the
factual truth of human hardware claims. Evidence review remains mandatory.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import sys

GATES = (
    "integrated_source_ci",
    "release_artifacts_modules",
    "native_gb_gbc_gba_regression",
    "sram_state_rtc_integrity",
    "save_restore_interruption",
    "updater_powerloss_recovery",
    "sleep_lid_resume_soak",
    "gpu_cold_start_frame_pacing",
    "audio_volume_resume",
    "battery_charge_thermal",
    "wifi_setup_time_rtc",
    "diagnostic_review_and_privacy",
)
VERSION_RE = re.compile(r"[0-9]+\.[0-9]+\.[0-9]+(?:-[0-9A-Za-z.]+)?\Z")
SHA_RE = re.compile(r"[0-9a-f]{64}\Z")
COMMIT_RE = re.compile(r"[0-9a-f]{40}\Z")
MAX_MANIFEST = 64 * 1024
MAX_PROOF_FILE = 20 * 1024 * 1024
MAX_ARCHIVE_FILE = 256 * 1024 * 1024
MAX_PROOF_PATH = 200


class GateError(ValueError):
    pass


def no_duplicate_pairs(pairs):
    values = {}
    for key, value in pairs:
        if key in values:
            raise GateError("duplicate JSON key")
        values[key] = value
    return values


def load_manifest(path):
    if path.is_symlink() or not path.is_file() or path.stat().st_size > MAX_MANIFEST:
        raise GateError("missing or oversized manifest")
    try:
        data = json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=no_duplicate_pairs)
    except (UnicodeError, json.JSONDecodeError) as exc:
        raise GateError("malformed manifest JSON") from exc
    if type(data) is not dict or set(data) != {"format", "version", "source_sha", "artifacts", "gates"}:
        raise GateError("manifest schema mismatch")
    return data


def read_regular(root, rel, max_bytes):
    if not isinstance(rel, str) or len(rel) > MAX_PROOF_PATH or not rel:
        raise GateError("invalid evidence path")
    p = Path(rel)
    if p.is_absolute() or any(part in ("..", ".") for part in p.parts) or len(p.parts) > 12:
        raise GateError("unsafe evidence path")
    if any(part in ("", "..") for part in rel.split("/")) or "\\" in rel:
        raise GateError("unsafe evidence path")
    # Never walk through a symlink, even if it resolves back inside the root.
    cur = root
    for part in p.parts:
        cur = cur / part
        if cur.is_symlink():
            raise GateError("symlinked evidence path")
    flags = os.O_RDONLY | getattr(os, "O_CLOEXEC", 0) | getattr(os, "O_NOFOLLOW", 0) | getattr(os, "O_NONBLOCK", 0)
    try:
        fd = os.open(cur, flags)
    except OSError as exc:
        raise GateError("missing or unreadable evidence file") from exc
    try:
        meta = os.fstat(fd)
        if not stat.S_ISREG(meta.st_mode) or meta.st_size == 0 or meta.st_size > max_bytes:
            raise GateError("unsafe or empty evidence file")
        digest = hashlib.sha256()
        total = 0
        while True:
            piece = os.read(fd, min(65536, max_bytes + 1 - total))
            if not piece:
                break
            total += len(piece)
            if total > max_bytes:
                raise GateError("evidence file too large")
            digest.update(piece)
        if total != meta.st_size:
            raise GateError("evidence changed while reading")
        return digest.hexdigest()
    finally:
        os.close(fd)


def checked_proof(entry, root, max_bytes=MAX_PROOF_FILE):
    if type(entry) is not dict or set(entry) != {"path", "sha256"}:
        raise GateError("proof schema mismatch")
    expected = entry["sha256"]
    if not isinstance(expected, str) or not SHA_RE.fullmatch(expected):
        raise GateError("invalid proof hash")
    return read_regular(root, entry["path"], max_bytes) == expected


def evaluate(manifest, root, version, source_sha):
    problems = []
    if type(manifest["format"]) is not int or manifest["format"] != 1:
        problems.append("unsupported manifest version")
    if not isinstance(manifest["version"], str) or not VERSION_RE.fullmatch(manifest["version"]) or manifest["version"] != version:
        problems.append("release version mismatch")
    if not isinstance(manifest["source_sha"], str) or not COMMIT_RE.fullmatch(manifest["source_sha"]) or manifest["source_sha"] != source_sha:
        problems.append("release commit mismatch")
    artifacts = manifest["artifacts"]
    if type(artifacts) is not dict or set(artifacts) != {"update", "fresh_install"}:
        problems.append("missing artifact records")
    else:
        for name in ("update", "fresh_install"):
            try:
                if not checked_proof(artifacts[name], root, MAX_ARCHIVE_FILE):
                    problems.append(f"{name}: hash mismatch")
            except GateError as exc:
                problems.append(f"{name}: {exc}")
    gates = manifest["gates"]
    if type(gates) is not dict or set(gates) != set(GATES):
        problems.append("required gate set mismatch")
        gates = {}
    passed = []
    pending = []
    for name in GATES:
        rec = gates.get(name)
        if type(rec) is not dict or set(rec) != {"status", "proofs"}:
            pending.append(name)
            continue
        if rec["status"] != "passed" or type(rec["proofs"]) is not list or not 1 <= len(rec["proofs"]) <= 8:
            pending.append(name)
            continue
        try:
            if all(checked_proof(p, root) for p in rec["proofs"]):
                passed.append(name)
            else:
                pending.append(name)
                problems.append(f"{name}: proof hash mismatch")
        except GateError as exc:
            pending.append(name)
            problems.append(f"{name}: {exc}")
    return {"ready": not problems and not pending,
            "passed": passed, "pending": pending, "problems": problems,
            "gate_count": len(GATES)}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--root", type=Path, required=True, help="Local evidence root, not a public GitHub checkout")
    parser.add_argument("--version", required=True, help="Expected package version independently read from VERSION")
    parser.add_argument("--source-sha", required=True, help="Expected exact Git commit (40 lowercase hex)")
    parser.add_argument("--audit", action="store_true", help="Report blockers without failing CI; NEVER authorises release")
    args = parser.parse_args(argv)
    if not VERSION_RE.fullmatch(args.version) or not COMMIT_RE.fullmatch(args.source_sha):
        parser.error("expected version and full source SHA required")
    try:
        manifest = load_manifest(args.manifest)
        result = evaluate(manifest, args.root, args.version, args.source_sha)
    except (OSError, GateError) as exc:
        result = {"ready": False, "passed": [], "pending": list(GATES), "problems": [str(exc)], "gate_count": len(GATES)}
    print(json.dumps(result, indent=2))
    return 0 if result["ready"] or args.audit else 1


if __name__ == "__main__":
    sys.exit(main())