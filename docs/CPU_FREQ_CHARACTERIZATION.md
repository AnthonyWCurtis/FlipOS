# CPU Frequency Characterization

## Purpose

Establish safe CPU frequency ceilings before any voltage changes. These tests
use the stock RK3566 OPP voltage table and only modify the kernel cpufreq
maximum frequency. Every test restores the stock 1.992 GHz ceiling afterward.

## 2026-10-06 - Pokemon LeafGreen (GBA / mGBA)

Device:
- Miyoo Flip RK3566
- CPU governor: ondemand
- Brightness: 200
- Volume: 65%
- GPU: simple_ondemand, 200 MHz during test
- USB connected: battery/current/power readings are not valid whole-device
  discharge measurements and must not be used for battery-life claims.

### 30-second ceiling sweep

| Max CPU | Mean CPU | CPU load | Mean SoC | FPS result | Health |
| ---: | ---: | ---: | ---: | --- | --- |
| 1992 MHz | 1301.6 MHz | 22.21% | 45.01 C | ~59.73 | clean |
| 1800 MHz | 1206.4 MHz | 21.57% | 44.93 C | ~59.73 | clean |
| 1608 MHz | 1274.4 MHz | 22.10% | 44.81 C | ~59.73 | clean |
| 1416 MHz | 1180.8 MHz | 22.62% | 44.46 C | ~59.73 | clean |
| 1104 MHz | 1094.4 MHz | 21.74% | 43.94 C | ~59.73 | clean |
| 816 MHz | 816.0 MHz | 27.05% | 43.85 C | ~59.73 | clean |
| 600 MHz | 600.0 MHz | 33.82% | 43.62 C | 58.60-59.64 | degraded |

The 600 MHz ceiling is below the practical floor for LeafGreen and is rejected.

### Five-minute active-play validation at 1.104 GHz

A longer test was performed while actively playing LeafGreen through ordinary
movement, menus, transitions and gameplay.

- 300 telemetry samples over 299 seconds
- CPU mean: 1061.76 MHz
- CPU range: 816-1104 MHz
- CPU load mean: 21.62%
- SoC mean: 43.98 C
- SoC range: 43.75-44.38 C
- GPU mean: 4.64% at 200 MHz
- 299 FPS samples
- FPS minimum: 59.700
- FPS mean: 59.727
- FPS maximum: 59.755
- no audio queue drops
- no GPU invalid events
- no crash/error/warning events

### Interpretation

1. The stock 1.992 GHz ceiling is unnecessary for this GBA workload.
2. 1.104 GHz is a strong provisional GBA ceiling: it retains full-speed
   emulation with substantial frequency headroom removed.
3. 1.104 GHz also remains on the current unit's 850 mV CPU OPP, avoiding the
   higher-voltage OPPs beginning at 1.416 GHz.
4. 816 MHz is promising for this title but has less safety margin and requires
   broader game/content testing before consideration for a release profile.
5. 600 MHz is measurably too low.
6. USB-connected current/power values are charger-side observations and cannot
   quantify battery-life improvement. Controlled unplugged A/B testing is
   required.

## Next validation

- Repeat 1.104 GHz GBA testing across additional titles/workloads.
- Test demanding scenes, long sessions, sleep/resume and GameSwitcher at the
  candidate ceiling.
- Characterize GB/GBC separately; do not assume the GBA ceiling applies.
- Only after frequency profiles are stable begin voltage calibration work.

## 2026-10-06 - Golden Sun: The Lost Age

Golden Sun: The Lost Age was added as a second, more demanding GBA workload.
The test was performed during early-game/normal-play activity. This is useful
evidence but does not yet cover the heaviest later-game Psynergy or battle
scenes.

### Stock baseline - 1.992 GHz ceiling

- 120 telemetry samples over 119 seconds
- CPU mean: 1298.00 MHz
- CPU load mean: 20.55%
- SoC mean: 45.77 C
- FPS samples: 120
- FPS minimum: 59.694
- FPS mean: 59.728
- FPS maximum: 59.753
- no audio queue drops
- no GPU invalid events
- no crash/error/warning events

### Candidate profile - 1.104 GHz ceiling

- 180 telemetry samples over 179 seconds
- CPU mean: 1011.20 MHz
- CPU load mean: 21.80%
- SoC mean: 45.54 C
- FPS samples: 179
- FPS minimum: 59.703
- FPS mean: 59.728
- FPS maximum: 59.756
- no audio queue drops
- no GPU invalid events
- no crash/error/warning events

The 1.104 GHz capped result is statistically indistinguishable from the stock
run for this captured workload.

### Experimental probe - 816 MHz ceiling

- 60 telemetry samples over 59 seconds
- CPU mean: 812.40 MHz
- CPU load mean: 26.63%
- SoC mean: 46.07 C
- FPS samples: 60
- FPS minimum: 59.713
- FPS mean: 59.727
- FPS maximum: 59.745
- no audio queue drops
- no GPU invalid events
- no crash/error/warning events

816 MHz also retained full-speed emulation during this short sample. It remains
experimental because the reduced frequency margin has not yet been tested
across demanding late-game scenes or a sufficiently broad GBA title set.

### Current GBA decision

Keep 1.104 GHz as the provisional-safe GBA ceiling. It removes access to all
higher-voltage CPU OPPs on the current unit while retaining significantly more
frequency margin than 816 MHz. Do not promote 816 MHz to the release profile
without broader and longer validation.
