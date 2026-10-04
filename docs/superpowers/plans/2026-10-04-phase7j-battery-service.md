# Phase 7J Formal Battery Service Implementation Plan

Date: 2026-10-04

## Task 1 — Shared Battery API

- Add `flip_battery.h` / `flip_battery.c`.
- Read percentage, status, voltage, current and temperature.
- Give every optional field an independent validity flag.
- Treat missing optional values as nonfatal.
- Add focused host tests using an overridden battery root.

## Task 2 — Telemetry integration

- Remove Telemetry's private battery path discovery and storage.
- Read battery percentage through `flip_battery_read()`.
- Preserve the existing telemetry overlay contract.
- Add architecture guard and verify-target wiring.

## Task 3 — Gates

Focused development gate:

```text
make test-battery test-telemetry
python3 tests/test_battery_architecture.py
make frontend gb
```

Pre-hardware gate:

```text
make verify
sha256sum build/flip-frontend build/flip-gb
```

Hardware acceptance:

- preserve Phase 7I rollback binaries;
- deploy the exact verified pair;
- run a small Battery service probe on the real Flip;
- compare percentage/status/voltage/current/temp with raw sysfs;
- verify Pokemon Red remains healthy and Telemetry still reports the battery source/percentage.

No physical lid or button regression is required unless hardware evidence exposes an unrelated lifecycle regression.
