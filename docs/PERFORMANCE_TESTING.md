# FlipOS Performance and Power Testing

This document defines the repeatable hardware workflow used before changing
CPU/GPU governor policy, suspend behaviour, audio latency, or other
performance/power settings.

## Principle

Measure first. Performance and power changes must be compared against a
repeatable baseline on the same Miyoo Flip, with the same brightness, volume,
ROM workload, capture duration, and power-source state.

Do not use shell loops that repeatedly spawn utilities to sample sysfs. On the
RK3566, that observer overhead is large enough to raise CPU utilisation and
drive the `ondemand` governor to higher frequencies.

## Capture tools

Build the low-overhead ARM64 sampler:

```sh
make perf-capture
```

The device utility is `build/flip-perf-capture`. Its usage is:

```
flip-perf-capture LABEL [DURATION_SECONDS] [INTERVAL_SECONDS] [OUTPUT.csv]
```

It records:

- actual live mode and emulator process
- session metadata
- saved volume and brightness plus actual backlight brightness
- aggregate CPU utilisation, CPU clock and governor
- GPU utilisation, GPU clock and governor
- SoC and GPU temperatures
- battery percentage, status, voltage, current and calculated power
- charge counter where supported
- frontend, GPU service and audio process presence

The sampler performs direct file/proc reads and does not spawn a subprocess per
sample.

Summarise a capture on the Linux development host:

```sh
python3 tools/flip-perf-summary.py capture.csv
```

The summary includes mean/min/max utilisation, clocks and temperatures plus
integrated discharge estimates in mAh and mWh.

## Standard performance baseline

Use USB-connected captures for scheduler, clock, frame pacing and thermal
comparison. Battery current while USB is connected is not a device-drain
measurement.

Default controlled conditions:

- brightness: 200 / 255
- game volume: 20%
- performance capture: 60 seconds at 1-second intervals
- allow a game to settle for at least 15 seconds before capture
- no manual button input during a capture unless the test explicitly measures it

Capture these workloads:

1. frontend idle
2. GB — Pokémon Blue
3. GBC — Pokémon Crystal
4. GBA — Pokémon FireRed

Emulator FPS/audio/GPU-host diagnostics remain available in
`flipos-dev/logs/frontend.log` and should be checked alongside the CSV.

## Standard unplugged power baseline

True battery tests must run with USB power disconnected.

Before each run:

1. set brightness to 200 / 255
2. set volume to 20% for active-game workloads
3. start the intended workload and allow it to settle
4. start `flip-perf-capture` detached
5. unplug USB only after capture has started
6. leave the device untouched for the requested duration
7. reconnect USB and retrieve the CSV

For longer power runs, use a 5-second interval to minimise measurement overhead.
A useful initial run is 10–15 minutes per workload.

The same matrix should eventually include frontend idle and lid-closed suspend.

## Acceptance rule for tuning

A governor/power change is accepted only when the relevant workload keeps:

- correct delivered emulator FPS
- no sustained audio queue drops or underruns
- no new GPU protocol errors
- no lifecycle/save-state regression
- acceptable temperature
- equal or lower measured energy use for the intended workload

Run focused tests during development. Run full `make verify` once at the stable
checkpoint before committing/merging the tuning change.

## Initial frontend idle reference

Captured on 2026-10-05 with USB connected, brightness 200 and no game running:

- aggregate CPU: 0.73% mean
- CPU clock: 408 MHz mean/min/max
- GPU load: 0%
- GPU clock: 200 MHz
- SoC temperature: 42.12 C mean
- GPU temperature: 40.98 C mean

The USB-connected battery current is intentionally not treated as a power-drain
baseline.

## 2026-10-06 — FFVI unplugged stock vs dynamic ABBA result

A controlled unplugged ABBA run compared the stock 1.992 GHz maximum policy
against the production GBA dynamic profile using the same Final Fantasy VI
Advance saved state, brightness 200, volume 15, shared GPU path and five-minute
capture windows. The order was stock A1 -> dynamic B1 -> dynamic B2 -> stock A2
to reduce first-to-last battery and thermal drift.

Individual five-minute capture results:

| Leg | Policy | Mean CPU | Mean battery power | Mean SoC temp | Final FPS |
| --- | --- | ---: | ---: | ---: | ---: |
| stock A1 | ondemand, max 1.992 GHz | 1.470 GHz | 2769.58 mW | 44.71 C | 59.722 |
| dynamic B1 | ondemand, max 1.104 GHz | 1.104 GHz | 2582.70 mW | 43.25 C | 59.735 |
| dynamic B2 | ondemand, max 1.104 GHz | 1.104 GHz | 2586.35 mW | 43.19 C | 59.724 |
| stock A2 | ondemand, max 1.992 GHz | 1.488 GHz | 2783.03 mW | 45.25 C | 59.728 |

Paired averages:

- stock battery power: 2776.31 mW
- dynamic battery power: 2584.52 mW
- whole-device saving: 191.78 mW / 6.91%
- stock battery current: 694.41 mA
- dynamic battery current: 645.85 mA / 6.99% lower
- stock mean CPU clock: 1.479 GHz
- dynamic mean CPU clock: 1.104 GHz / 25.37% lower
- stock mean SoC temperature: 44.98 C
- dynamic mean SoC temperature: 43.22 C / 1.76 C cooler
- stock mean GPU temperature: 43.10 C
- dynamic mean GPU temperature: 41.89 C / 1.21 C cooler
- integrated energy per 295-second capture averaged about 227.52 mWh stock
  versus 211.76 mWh dynamic, a 6.93% reduction

Performance acceptance also passed. Across all four emulator logs, mean delivered
FPS remained 59.727-59.728, audio queue dropped frames stayed at zero, timing
resyncs stayed at zero, the shared GPU path remained active, and no emulator
errors appeared. One startup underrun occurred in each leg equally and is not a
policy-specific regression. Neither dynamic leg required an upward clock raise.

For this exact FFVI workload, the measured power ratio corresponds to roughly
7.4% more runtime if battery capacity and all other loads remain equal. That is
a workload-specific projection, not a claim that every game or system gains
7.4% battery life. Longer mixed-workload and suspend measurements remain useful
for release-level battery-life estimates.
