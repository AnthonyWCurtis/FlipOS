#!/usr/bin/env python3
"""Create an explicitly requested, local, metrics-only FlipOS diagnostic report.

Developer-host utility. Not installed on Miyoo Flip. Never uploads anything.
The output is not a substitute for the user's review before they share it.
"""

from __future__ import annotations

import argparse
import json
import math
import os
from pathlib import Path
import re
import stat

ALLOWED_FILES = frozenset({
    "frontend.log", "frontend.previous.log", "gpu.log", "emulator.log",
    "battery.log", "error.log",
})
MAX_SOURCES = 6
MAX_SOURCE_SIZE = 32 * 1024 * 1024
MAX_TAIL_BYTES = 256 * 1024
MAX_EVENTS_PER_SOURCE = 60
MAX_OUTPUT_BYTES = 128 * 1024

# Strictly allowlisted fields, not a best-effort redaction of arbitrary log text.
# These are numerical metrics from the published FlipOS logging code.
METRICS = {
    "FLIP_PERF": frozenset({
        "fps", "frames", "input_avg_us", "input_max_us",
        "controls_avg_us", "controls_max_us", "core_input_avg_us",
        "core_input_max_us", "core_run_avg_us", "core_run_max_us",
        "audio_avg_us", "audio_max_us", "host_avg_us", "host_max_us",
        "late_avg_us", "late_max_us", "late_frames", "resyncs",
    }),
    "FLIP_AUDIO_PERF": frozenset({
        "submits", "submit_avg_us", "submit_max_us", "writes",
        "write_avg_us", "write_max_us", "write_total_us",
        "queue_peak_frames", "queue_dropped_frames",
    }),
    "FLIP_GPU_SERVICE_PERF": frozenset({
        "frames", "overlays", "render_avg_us", "render_max_us",
        "draw_cpu_avg_us", "swap_present_avg_us", "swap_present_max_us",
        "invalid",
    }),
    "FLIP_TELEMETRY": frozenset({
        "fps", "cpu", "cpu_mhz", "gpu", "gpu_mhz", "temp_c", "bat",
    }),
    "FLIP_CPU_DYNAMIC": frozenset({
        "fps", "target", "host_avg_us", "late_max_us", "resyncs",
        "audio_drops", "raise_count", "lower_count", "good_windows",
        "learned_floor_khz",
    }),
    "FLIP_CORE_HOST_FPS": frozenset({"fps", "target"}),
}
NUMERIC = re.compile(r"-?(?:\d+(?:\.\d+)?|\.\d+)(?:[eE][+-]?\d+)?\Z")
FIELDS = re.compile(r"([a-z][a-z0-9_]*)=([^\s]+)")
ALERT = re.compile(r"(?:\b|_)(?:FAILED|FAILURE|ERROR|CRASH|SEGFAULT|FATAL)\b", re.I)


def _safe_tail(path: Path) -> tuple[bytes, bool]:
    flags = os.O_RDONLY | getattr(os, "O_CLOEXEC", 0) | getattr(os, "O_NOFOLLOW", 0)
    flags |= getattr(os, "O_NONBLOCK", 0)
    fd = os.open(path, flags)
    try:
        info = os.fstat(fd)
        if not stat.S_ISREG(info.st_mode):
            raise ValueError("diagnostic source is not a regular file")
        if info.st_size > MAX_SOURCE_SIZE:
            raise ValueError("diagnostic source is too large")
        trimmed = info.st_size > MAX_TAIL_BYTES
        os.lseek(fd, max(0, info.st_size - MAX_TAIL_BYTES), os.SEEK_SET)
        result = bytearray()
        while len(result) < MAX_TAIL_BYTES:
            part = os.read(fd, MAX_TAIL_BYTES - len(result))
            if not part:
                break
            result.extend(part)
        if trimmed:
            # The tail starts in the middle of a line: omit that fragment.
            newline = result.find(b"\n")
            result = result[newline + 1:] if newline >= 0 else bytearray()
        return bytes(result), trimmed
    finally:
        os.close(fd)


def _number(token: str) -> int | float | None:
    if len(token) > 32 or not NUMERIC.fullmatch(token):
        return None
    try:
        value = float(token)
    except (ValueError, OverflowError):
        return None
    if not math.isfinite(value) or abs(value) > 1e13:
        return None
    if "." not in token and "e" not in token.lower():
        return int(token)
    return value


def _event(line: str) -> dict | None:
    prefix, sep, rest = line.partition(": ")
    if not sep:
        prefix, sep, rest = line.partition(" ")
    if not sep or prefix not in METRICS:
        return None
    permitted = METRICS[prefix]
    found = {}
    if prefix == "FLIP_CORE_HOST_FPS":
        first, _, remainder = rest.partition(" ")
        num = _number(first)
        if num is not None:
            found["fps"] = num
        rest = remainder
    for match in FIELDS.finditer(rest):
        key = match.group(1)
        if key in permitted:
            num = _number(match.group(2))
            if num is not None:
                found[key] = num
    return {"kind": prefix, "metrics": found} if found else None


def collect(paths: list[Path]) -> dict:
    if not 1 <= len(paths) <= MAX_SOURCES:
        raise ValueError("select between 1 and 6 logs")
    if len({str(p.absolute()) for p in paths}) != len(paths):
        raise ValueError("duplicate source")
    report = {
        "format": "FLIPOS_METRICS_ONLY_V1",
        "automatically_sent": False,
        "raw_log_lines_included": False,
        "sources": [],
    }
    for path in paths:
        if path.name not in ALLOWED_FILES:
            raise ValueError("source log name is not allowlisted")
        raw, trimmed = _safe_tail(path)
        entries = raw.decode("utf-8", errors="replace").splitlines()
        source = {
            "name": path.name,
            "tail_truncated": trimmed,
            "lines_examined": len(entries),
            "alert_lines_count": 0,
            "events": [],
        }
        for line in entries:
            if ALERT.search(line):
                source["alert_lines_count"] += 1
            if len(source["events"]) >= MAX_EVENTS_PER_SOURCE:
                continue
            event = _event(line)
            if event is not None:
                source["events"].append(event)
        report["sources"].append(source)
    return report


def export(paths: list[Path], destination: Path, *, consent: bool = False) -> int:
    if not consent:
        raise PermissionError("explicit --consent is required")
    report = collect(paths)
    output = (json.dumps(report, indent=2, ensure_ascii=True) + "\n").encode("utf-8")
    if len(output) > MAX_OUTPUT_BYTES:
        raise ValueError("diagnostic report exceeds safe output limit")
    fd = os.open(destination, os.O_CREAT | os.O_EXCL | os.O_WRONLY |
                 getattr(os, "O_CLOEXEC", 0), 0o600)
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(output)
            stream.flush()
            os.fsync(stream.fileno())
    except BaseException:
        destination.unlink(missing_ok=True)
        raise
    return len(output)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", action="append", type=Path, required=True,
                        help="Local FlipOS diagnostic log (repeat for multiple files)")
    parser.add_argument("--output", type=Path, required=True,
                        help="New local JSON file; existing files are never overwritten")
    parser.add_argument("--consent", action="store_true",
                        help="Explicitly authorise creation of a local diagnostic report")
    args = parser.parse_args()
    try:
        count = export(args.input, args.output, consent=args.consent)
    except (OSError, ValueError, PermissionError) as error:
        parser.exit(1, f"diagnostic export refused: {error}\n")
    print(f"Created {args.output.name} ({count} bytes). Review it before sharing; no data sent.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
