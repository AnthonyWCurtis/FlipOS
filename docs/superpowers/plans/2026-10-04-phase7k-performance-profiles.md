# Phase 7K Performance Profiles Service Implementation Plan

Date: 2026-10-04

## Task 1 — Shared governor service

- Add `flip_performance.h` / `flip_performance.c`.
- Define BALANCED, PERFORMANCE and POWERSAVE.
- Validate requested governors against each hardware available-governor list.
- Add query, apply and exact restore APIs.
- Roll CPU back automatically if a later GPU write fails.

## Task 2 — Guardrails

- Keep governor sysfs roots overridable for host tests.
- Do not change min/max frequencies.
- Do not add automatic profile selection.
- Wire focused tests and architecture guard into verification.

## Task 3 — Gates

Focused gate:

```text
make test-performance
python3 tests/test_performance_architecture.py
make frontend gb
```

Pre-hardware gate:

```text
make verify
sha256sum build/flip-frontend build/flip-gb
```

Hardware acceptance:

- capture current CPU/GPU governors;
- verify all three profiles report supported;
- apply PERFORMANCE and confirm both governors;
- apply POWERSAVE and confirm both governors;
- restore the exact starting governors;
- deploy verified binaries with Phase 7J rollback copies preserved;
- confirm normal frontend boot.

No gameplay or physical-lid regression is required because Phase 7K does not automatically apply a profile in production.
