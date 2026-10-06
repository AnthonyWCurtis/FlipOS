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

## 2026-10-06 - Mother 3

Mother 3 was added as a third GBA workload. The run covered normal early-game
play. Later battle/effect-heavy sections should still be tested separately.

### Stock baseline - 1.992 GHz ceiling

- 90 telemetry samples over 89 seconds
- CPU mean: 1142.67 MHz
- CPU load mean: 16.56%
- SoC mean: 47.75 C
- FPS samples: 90
- FPS minimum: 59.712
- FPS mean: 59.727
- FPS maximum: 59.743
- no audio queue drops
- no GPU invalid events
- no crash/error/warning events

### Candidate profile - 1.104 GHz ceiling

- 180 telemetry samples over 179 seconds
- CPU mean: 976.80 MHz
- CPU load mean: 16.26%
- SoC mean: 47.47 C
- FPS samples: 179
- FPS minimum: 59.710
- FPS mean: 59.728
- FPS maximum: 59.753
- no audio queue drops
- no GPU invalid events
- no crash/error/warning events

The 1.104 GHz result again matches stock performance for the captured workload.

### Experimental probe - 816 MHz ceiling

- 60 telemetry samples over 59 seconds
- CPU mean: 816.00 MHz
- CPU load mean: 19.01%
- SoC mean: 47.75 C
- FPS samples: 60
- FPS minimum: 59.718
- FPS mean: 59.727
- FPS maximum: 59.740
- no audio queue drops
- no GPU invalid events
- no crash/error/warning events

816 MHz remains promising but experimental. The provisional-safe GBA ceiling
remains 1.104 GHz until broader title coverage and demanding later-game scenes
are validated.

## 2026-10-06 - Final Fantasy VI Advance

Final Fantasy VI Advance was used as the fourth GBA workload and produced the
highest temperatures of the title set. It also established the practical
frequency floor more clearly than the previous games.

### Stock baseline - 1.992 GHz ceiling

- 90 telemetry samples over 89 seconds
- CPU mean: 1411.73 MHz
- CPU load mean: 21.66%
- SoC mean: 49.88 C
- SoC range: 48.89-50.62 C
- FPS samples: 89
- FPS minimum: 59.702
- FPS mean: 59.728
- FPS maximum: 59.754
- no audio queue drops
- no GPU invalid events
- no crash/error/warning events

### Candidate profile - 1.104 GHz ceiling

- 180 telemetry samples over 179 seconds
- CPU mean: 1086.80 MHz
- CPU load mean: 22.90%
- SoC mean: 49.27 C
- SoC range: 48.33-50.00 C
- FPS samples: 179
- FPS minimum: 59.662
- FPS mean: 59.728
- FPS maximum: 59.791
- no audio queue drops
- no GPU invalid events
- no crash/error/warning events

The brief minimum/maximum FPS excursion was isolated and self-compensating;
the mean remained exactly on target and subsequent samples returned immediately
to normal pacing. There was no corresponding audio, GPU, crash or warning
event.

### Experimental 816 MHz probe - rejected for GBA release profile

During a heavier Final Fantasy VI scene, the 816 MHz ceiling produced immediate
and sustained slowdown. Observed host FPS fell from the 59.728 target into
approximately the 53.6-54.3 FPS range and later around 51.5-52.2 FPS before the
test was aborted. The CPU ceiling was immediately restored to stock and FPS
recovered to approximately 59.72-59.73.

This demonstrates that the earlier successful 816 MHz probes in LeafGreen,
Golden Sun and Mother 3 did not provide enough workload coverage to make 816
MHz a safe global GBA profile.

### GBA frequency decision

1.104 GHz is now the provisional-safe GBA ceiling across the tested title set:

- Pokemon LeafGreen
- Golden Sun: The Lost Age
- Mother 3
- Final Fantasy VI Advance

It retains full-speed emulation in all four tested workloads while avoiding the
higher-voltage OPPs that begin at 1.416 GHz on this Flip. 816 MHz is explicitly
rejected as a general GBA release ceiling because Final Fantasy VI can exceed
its available CPU headroom.

## 2026-10-06 - Gambatte GB/GBC minimum-OPP characterization

Available GB/GBC ROMs on the test card are currently limited to Pokemon titles,
so this result is a provisional starting-ceiling decision rather than a
universal Gambatte guarantee.

### Pokemon Blue - 408 MHz ceiling

A short hardware run at the RK3566 minimum OPP completed at target speed with
zero audio-drop windows and zero timing resyncs. One early sample reached
59.291 FPS, while the short-run average remained at the expected ~59.727 FPS.

A longer 45-second follow-up removed concern that the low sample represented a
sustained limit:

- 44 measured FPS windows
- minimum 59.715 FPS
- average 59.727 FPS
- maximum 59.741 FPS
- audio-drop windows: 0
- frame-schedule resyncs: 0
- representative core-run averages remained roughly 4.5-4.7 ms

### Pokemon Crystal - 408 MHz ceiling

A controlled hardware run at 408 MHz also remained full speed:

- 19 measured FPS windows
- minimum 59.713 FPS
- average 59.727 FPS
- maximum 59.742 FPS
- audio-drop windows: 0
- frame-schedule resyncs: 0

### GB/GBC starting-ceiling decision

408 MHz is the provisional FlipOS starting ceiling for both GB and GBC on this
hardware. It is the lowest available RK3566 CPU OPP, so no lower fixed ceiling
exists to characterize.

This does not claim every Gambatte title is proven at 408 MHz. FlipOS therefore
uses the same upward-only Dynamic CPU Performance Controller as GBA: GB/GBC
sessions may start at 408 MHz, while the emulator host can raise one valid OPP
at a time if measured FPS or audio pressure shows that a future workload needs
more headroom. The pre-launch maximum remains the absolute escalation limit and
is restored when the session returns to the frontend.
