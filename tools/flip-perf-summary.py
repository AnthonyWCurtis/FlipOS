#!/usr/bin/env python3
"""Summarise FlipOS performance/power CSV captures."""

from __future__ import annotations

import csv
import math
import statistics
import sys
from collections import Counter
from pathlib import Path


def number(row: dict[str, str], key: str) -> float | None:
    text = row.get(key, "").strip()
    if not text:
        return None
    try:
        value = float(text)
    except ValueError:
        return None
    return value if math.isfinite(value) else None


def values(rows: list[dict[str, str]], key: str) -> list[float]:
    return [value for row in rows if (value := number(row, key)) is not None]


def stats(rows: list[dict[str, str]], key: str, scale: float = 1.0) -> str:
    data = [value / scale for value in values(rows, key)]
    if not data:
        return "--"
    return (
        f"mean={statistics.fmean(data):.2f} "
        f"min={min(data):.2f} max={max(data):.2f}"
    )


def first_last(rows: list[dict[str, str]], key: str) -> tuple[float | None, float | None]:
    data = values(rows, key)
    if not data:
        return None, None
    return data[0], data[-1]


def integrate_discharge(rows: list[dict[str, str]]) -> tuple[float, float]:
    """Return estimated discharge (mAh, mWh) from signed battery samples."""
    mah = 0.0
    mwh = 0.0

    for previous, current in zip(rows, rows[1:]):
        t0 = number(previous, "epoch")
        t1 = number(current, "epoch")
        ua = number(previous, "battery_ua")
        mw = number(previous, "battery_power_mw")

        if t0 is None or t1 is None or t1 <= t0:
            continue

        dt = t1 - t0

        # The Miyoo Flip reports discharge as negative current/power.
        if ua is not None and ua < 0:
            mah += (-ua * dt) / 3_600_000.0
        if mw is not None and mw < 0:
            mwh += (-mw * dt) / 3600.0

    return mah, mwh


def summarise(path: Path) -> int:
    with path.open(newline="", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle))

    if not rows:
        print(f"{path}: no samples", file=sys.stderr)
        return 1

    required = {"epoch", "live_mode", "cpu_percent", "battery_ua"}
    missing = required.difference(rows[0])
    if missing:
        print(f"{path}: missing columns: {', '.join(sorted(missing))}", file=sys.stderr)
        return 1

    epochs = values(rows, "epoch")
    duration = (epochs[-1] - epochs[0]) if len(epochs) >= 2 else 0.0
    labels = Counter(row.get("label", "") for row in rows)
    modes = Counter(row.get("live_mode", "") for row in rows)
    systems = Counter(row.get("session_system", "") for row in rows if row.get("session_system"))
    volumes = Counter(row.get("volume_percent", "") for row in rows if row.get("volume_percent"))
    brightness = Counter(row.get("brightness_actual", "") for row in rows if row.get("brightness_actual"))

    battery_start, battery_end = first_last(rows, "battery_percent")
    voltage_start, voltage_end = first_last(rows, "battery_uv")
    discharge_mah, discharge_mwh = integrate_discharge(rows)

    print(f"Capture: {path}")
    print(f"Samples: {len(rows)} over {duration:.0f}s")
    print(f"Label: {', '.join(f'{k} ({v})' for k, v in labels.items())}")
    print(f"Live mode: {', '.join(f'{k} ({v})' for k, v in modes.items())}")
    if systems:
        print(f"Session system: {', '.join(f'{k} ({v})' for k, v in systems.items())}")
    if volumes:
        print(f"Volume %: {', '.join(f'{k} ({v})' for k, v in volumes.items())}")
    if brightness:
        print(f"Brightness: {', '.join(f'{k} ({v})' for k, v in brightness.items())}")

    print(f"CPU %: {stats(rows, 'cpu_percent')}")
    print(f"CPU MHz: {stats(rows, 'cpu_khz', 1000.0)}")
    print(f"GPU %: {stats(rows, 'gpu_percent')}")
    print(f"GPU MHz: {stats(rows, 'gpu_hz', 1_000_000.0)}")
    print(f"SoC C: {stats(rows, 'soc_temp_mc', 1000.0)}")
    print(f"GPU C: {stats(rows, 'gpu_temp_mc', 1000.0)}")
    print(f"Battery current mA: {stats(rows, 'battery_ua', 1000.0)}")
    print(f"Battery power mW: {stats(rows, 'battery_power_mw')}")

    if battery_start is not None and battery_end is not None:
        print(
            f"Battery %: {battery_start:.0f} -> {battery_end:.0f} "
            f"({battery_end - battery_start:+.0f})"
        )

    if voltage_start is not None and voltage_end is not None:
        print(
            f"Battery voltage: {voltage_start / 1_000_000.0:.3f}V -> "
            f"{voltage_end / 1_000_000.0:.3f}V"
        )

    print(f"Integrated discharge: {discharge_mah:.3f} mAh / {discharge_mwh:.3f} mWh")
    return 0


def main(argv: list[str]) -> int:
    if len(argv) < 2:
        print(f"usage: {argv[0]} CAPTURE.csv [CAPTURE.csv ...]", file=sys.stderr)
        return 2

    result = 0
    for index, name in enumerate(argv[1:]):
        if index:
            print()
        result |= summarise(Path(name))
    return result


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
