# FlipOS Phase 7J Battery Service Hardware Acceptance

Date: 2026-10-04
Device: Miyoo Flip, RK3566

## Candidate

Hardware-tested code commit: `9cfa447`

Deployed SHA-256:

```text
flip-frontend 12fe2894096fe6b2b47cb3d3840d9728006bb46ad504b680abb5373ba917fdd0
flip-gb       5fdb8d680ef987ca5ac30964d094f1f332c18492d51c34a527bd8e42500ab9d8
```

The previously accepted Phase 7I binaries were preserved on-device as rollback copies before replacement.

## Host gate

The exact deployed tree passed `make verify`.

Focused tests passed for:

- complete battery snapshots;
- missing optional fields;
- invalid/out-of-range fields;
- charge-status parsing;
- Telemetry integration;
- Battery architecture ownership.

## Direct hardware service proof

The Battery service probe returned:

```text
percent=100 percent_valid=1
status=FULL status_valid=1
voltage_uv=4238000 voltage_valid=1
current_ua=-5000 current_valid=1
temp_deci_c=188 temp_valid=1
```

The raw hardware sysfs values at the same time were:

```text
capacity=100
status=Full
voltage_now=4238000
current_now=-5000
temp=188
```

The service therefore matched every scoped hardware field exactly.

## Telemetry integration proof

A real-device Telemetry probe using the shared Battery service reported:

```text
FLIP_TELEMETRY: ... battery=/sys/class/power_supply/battery
battery_valid=1 battery_percent=100 overlay=BAT 100%
```

Raw capacity remained `100`.

Telemetry no longer owns a private battery sysfs discovery path.

## Reboot / frontend regression

After deploying the exact verified pair and rebooting:

- both deployed hashes matched;
- the frontend started normally;
- the previously running game session was correctly classified as `interrupted` by existing Session Manager recovery policy;
- no battery or telemetry startup failure occurred.

No physical lid/button regression was repeated because Phase 7J changes read-only battery observation and Telemetry plumbing, not lifecycle decisions.

## Result

Phase 7J Battery service is hardware accepted.

Battery sysfs parsing now has one shared owner, exposes richer validated state, and remains read-only. Telemetry consumes the shared service while preserving the established battery percentage overlay contract.
