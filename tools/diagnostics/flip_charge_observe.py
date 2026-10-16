#!/usr/bin/env python3
"""Local-only, read-only RK817 battery/charger observation and daemon identity audit.

Research utility, NOT included in FlipOS release binaries. It never uploads,
writes /sys, invokes the charger helper, restarts daemons or reads ROM paths.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import stat
import sys
import time

VERSION = "FLIPOS_CHARGE_OBSERVATION_V1"
MAX_SAMPLES = 120
MAX_VALUE_BYTES = 96
FIELDS = {
    "temp_deci_c": ("/sys/class/power_supply/battery/temp", -400, 1000),
    "battery_voltage_uv": ("/sys/class/power_supply/battery/voltage_now", 0, 10000000),
    "battery_current_ua": ("/sys/class/power_supply/battery/current_now", -10000000, 10000000),
    "capacity_percent": ("/sys/class/power_supply/battery/capacity", 0, 100),
    "usb_online": ("/sys/class/power_supply/usb/online", 0, 1),
    "ac_online": ("/sys/class/power_supply/ac/online", 0, 1),
    "requested_charger_limit_ma": (
        "/sys/module/flip_rk817_charger/parameters/current_limit_ma", -1, 3500),
}
STATUS_PATH = "/sys/class/power_supply/battery/status"
STATUS_ALLOWED = frozenset(("Charging", "Discharging", "Not charging", "Full", "Unknown"))
PID_PATH = "/tmp/flipos-charge-service.pid"
EXPECTED_EXE = "/mnt/SDCARD/flipos-dev/bin/flip-charge-service"
DIGITS = re.compile(r"[0-9]+\Z")


def path_at(root: Path, absolute: str) -> Path:
    return root / absolute.lstrip("/")


def read_field(path: Path) -> str | None:
    flags = os.O_RDONLY | getattr(os, "O_NONBLOCK", 0) | getattr(os, "O_NOFOLLOW", 0)
    flags |= getattr(os, "O_CLOEXEC", 0)
    try:
        fd = os.open(path, flags)
    except OSError:
        return None
    try:
        info = os.fstat(fd)
        if not stat.S_ISREG(info.st_mode):
            return None
        raw = os.read(fd, MAX_VALUE_BYTES + 1)
        if len(raw) > MAX_VALUE_BYTES:
            return None
        return raw.decode("ascii", errors="strict").strip()
    except (OSError, UnicodeError):
        return None
    finally:
        os.close(fd)


def read_int(root: Path, name: str) -> int | None:
    rel, minimum, maximum = FIELDS[name]
    value = read_field(path_at(root, rel))
    if value is None or not re.fullmatch(r"-?[0-9]+", value):
        return None
    try:
        number = int(value, 10)
    except ValueError:
        return None
    return number if minimum <= number <= maximum else None


def service_health(root: Path) -> str:
    """Identity check only. Never treats PID presence as proof of a live daemon."""
    raw = read_field(path_at(root, PID_PATH))
    if raw is None:
        return "pid_file_unavailable"
    if len(raw) > 10 or DIGITS.fullmatch(raw) is None:
        return "invalid_pid_file"
    pid = int(raw)
    if not (1 < pid <= 4194304):
        return "invalid_pid_file"
    process_exe = path_at(root, "/proc/" + str(pid) + "/exe")
    expected_exe = path_at(root, EXPECTED_EXE)
    try:
        proc = os.stat(process_exe)
    except FileNotFoundError:
        return "pid_not_running"
    except OSError:
        return "identity_unavailable"
    try:
        wanted = os.stat(expected_exe)
    except OSError:
        return "identity_unavailable"
    if not stat.S_ISREG(proc.st_mode) or not stat.S_ISREG(wanted.st_mode):
        return "identity_unavailable"
    return ("confirmed_process_identity" if
            (proc.st_dev, proc.st_ino) == (wanted.st_dev, wanted.st_ino)
            else "different_or_replaced_executable")


def collect(root: Path, elapsed_s: float) -> dict:
    sample = {
        "format": VERSION,
        "elapsed_s": round(elapsed_s, 3),
        "service_health": service_health(root),
        "battery_status": None,
    }
    for field in FIELDS:
        sample[field] = read_int(root, field)
    status = read_field(path_at(root, STATUS_PATH))
    if status in STATUS_ALLOWED:
        sample["battery_status"] = status
    # No ROM paths, SSID, process IDs, serials or raw kernel log lines.
    return sample


def validate(count: int, interval: float, fixture_root: Path | None) -> None:
    if not (1 <= count <= MAX_SAMPLES):
        raise ValueError("sample count must be 1..120")
    if not (0 <= interval <= 60):
        raise ValueError("interval must be 0..60 seconds")
    if fixture_root is None and interval < 5 and count > 1:
        raise ValueError("live multiple-sample reads require at least 5 seconds spacing")
    if fixture_root is not None and not fixture_root.is_absolute():
        raise ValueError("synthetic fixture root must be absolute")


def run(count: int, interval: float, fixture_root: Path | None, output) -> None:
    validate(count, interval, fixture_root)
    root = fixture_root if fixture_root is not None else Path("/")
    start = time.monotonic()
    for index in range(count):
        if index:
            time.sleep(interval)
        sample = collect(root, time.monotonic() - start)
        sample["source"] = "synthetic_fixture" if fixture_root is not None else "live_device"
        output.write(json.dumps(sample, sort_keys=True, separators=(",", ":")) + "\n")
        output.flush()


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--samples", type=int, default=1)
    parser.add_argument("--interval-seconds", type=float, default=10)
    parser.add_argument("--fixture-root", type=Path, help="host tests only; never a physical /sys override")
    args = parser.parse_args(argv)
    try:
        run(args.samples, args.interval_seconds, args.fixture_root, sys.stdout)
    except (OSError, ValueError) as exc:
        parser.exit(2, "readonly charge observation refused: " + str(exc) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
