#!/usr/bin/env python3
"""Opt-in local ADB read-only FlipOS charging inventory. Never writes to the device."""
from __future__ import annotations
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time

REMOTE_SH = r"""
set -eu
field() {
  key=$1
  path=$2
  if [ -f "$path" ] && [ -r "$path" ]; then
    value=$(head -n 1 "$path" 2>/dev/null | cut -c 1-80)
    printf '%s=%s\n' "$key" "$value"
  fi
}
field temp_deci_c /sys/class/power_supply/battery/temp
field current_ua /sys/class/power_supply/battery/current_now
field voltage_uv /sys/class/power_supply/battery/voltage_now
field percent /sys/class/power_supply/battery/capacity
field battery_status /sys/class/power_supply/battery/status
field usb_online /sys/class/power_supply/usb/online
field ac_online /sys/class/power_supply/ac/online
field requested_cap_ma /sys/module/flip_rk817_charger/parameters/current_limit_ma
field boot_stock_ma /sys/module/flip_rk817_charger/parameters/stock_current_ma
if grep -q '^flip_rk817_charger ' /proc/modules 2>/dev/null; then
  printf 'module_loaded=1\n'
else
  printf 'module_loaded=0\n'
fi
printf 'kernel_release=%s\n' "$(uname -r 2>/dev/null | cut -c 1-80)"
mod=/mnt/SDCARD/flipos-dev/lib/flip_rk817_charger.ko
if [ -r "$mod" ] && [ -f "$mod" ] && command -v sha256sum >/dev/null 2>&1; then
  sha256sum "$mod" 2>/dev/null | awk '{print "module_file_sha256=" $1}'
fi
"""

LIMITS = {
    "temp_deci_c": (-400, 1000), "current_ua": (-10000000, 10000000),
    "voltage_uv": (0, 10000000), "percent": (0, 100),
    "usb_online": (0, 1), "ac_online": (0, 1),
    "requested_cap_ma": (-1, 3500), "boot_stock_ma": (500, 3500),
    "module_loaded": (0, 1),
}
STATUSES = {"Charging", "Discharging", "Not charging", "Full", "Unknown"}
FIELDS = set(LIMITS) | {"battery_status", "kernel_release", "module_file_sha256"}
DEV_LINE = re.compile(r"^([^\s\t]+)\tdevice(?:\s|$)")
FIELD_LINE = re.compile(r"([a-z_]+)=([^\r\n]{0,80})\Z")


class CollectionError(RuntimeError):
    pass


def call(cmd: list[str], input_text: str | None = None) -> str:
    try:
        p = subprocess.run(cmd, input=input_text, capture_output=True,
                           text=True, timeout=18, check=False)
    except (OSError, subprocess.TimeoutExpired) as exc:
        raise CollectionError("ADB unavailable or timed out") from exc
    if p.returncode or len(p.stdout) > 8192:
        raise CollectionError("ADB read failed or returned excessive output")
    return p.stdout


def choose_device(adb: str, serial: str | None) -> str:
    devices = [m.group(1) for line in call([adb, "devices"]).splitlines()
               if (m := DEV_LINE.match(line))]
    if serial is not None:
        if serial not in devices:
            raise CollectionError("specified device is not authorised")
        return serial
    if len(devices) != 1:
        raise CollectionError("connect one authorised device or supply --serial")
    return devices[0]


def parse(text: str) -> dict:
    values = {k: None for k in FIELDS}
    seen = set()
    for line in text.splitlines():
        m = FIELD_LINE.fullmatch(line.rstrip("\r"))
        if not m:
            continue
        key, value = m.groups()
        if key not in FIELDS or key in seen:
            continue
        seen.add(key)
        if key in LIMITS:
            if not re.fullmatch(r"-?[0-9]{1,16}", value):
                continue
            v = int(value)
            lo, hi = LIMITS[key]
            if lo <= v <= hi:
                values[key] = v
        elif key == "battery_status" and value in STATUSES:
            values[key] = value
        elif key == "kernel_release" and re.fullmatch(r"[A-Za-z0-9._+~\-]{1,80}", value):
            values[key] = value
        elif key == "module_file_sha256" and re.fullmatch(r"[0-9a-f]{64}", value):
            values[key] = value
    return values


def collect(adb: str, count: int, interval: int, serial: str | None) -> list[dict]:
    if not 1 <= count <= 30 or not 0 <= interval <= 60:
        raise ValueError("invalid count/interval")
    if count > 1 and interval < 5:
        raise ValueError("live repeated reads must be at least 5 seconds apart")
    device = choose_device(adb, serial)
    entries = []
    for n in range(count):
        if n:
            time.sleep(interval)
        record = parse(call([adb, "-s", device, "shell", "sh", "-s"], REMOTE_SH))
        record["sample"] = n + 1
        record["utc"] = datetime.now(timezone.utc).isoformat(timespec="seconds")
        entries.append(record)
    return entries


def write_report(path: Path, entries: list[dict]) -> None:
    data = {"format": "FLIPOS_ADB_CHARGE_READONLY_V1",
            "measurements": entries, "uploaded": False, "device_writes": False,
            "thermal_cutoff_hardware_verified": False,
            "requested_cap_is_not_measured_current": True}
    raw = (json.dumps(data, indent=2, sort_keys=True) + "\n").encode()
    if len(raw) > 131072:
        raise CollectionError("report too large")
    flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL | getattr(os, "O_CLOEXEC", 0)
    fd = os.open(path, flags, 0o600)
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(raw)
            stream.flush()
            os.fsync(stream.fileno())
    except BaseException:
        path.unlink(missing_ok=True)
        raise


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--consent", action="store_true")
    p.add_argument("--adb", default="adb")
    p.add_argument("--serial")
    p.add_argument("--samples", type=int, default=6)
    p.add_argument("--interval-seconds", type=int, default=10)
    p.add_argument("--output", type=Path, required=True)
    args = p.parse_args(argv)
    if not args.consent:
        p.exit(2, "no collection performed: --consent required\n")
    try:
        entries = collect(args.adb, args.samples, args.interval_seconds, args.serial)
        write_report(args.output, entries)
    except (CollectionError, ValueError, OSError) as exc:
        p.exit(2, "read-only collection refused: " + str(exc) + "\n")
    print("Local report created. Review before sharing; nothing uploaded.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
