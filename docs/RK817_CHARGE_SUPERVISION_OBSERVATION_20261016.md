# RK817 charging supervision and read-only observation plan
16 October 2026 — **host-tested preparation, NOT Miyoo hardware acceptance**

## Source baseline and charging-driver evidence

The current published FlipOS \`main\` baseline is \`6d6eb28e93e1eee24cb9e0b23b64d3641c6bee19\`. It starts \`flip-charge-service\` from \`tools/device/updater-playtest\` and writes \`/tmp/flipos-charge-service.pid\`. That launcher currently uses \`kill -0\` alone to decide whether to skip launch. A reused PID can therefore be mistaken for a healthy charger process; there is no ongoing process watchdog.

The current published release packaging pins the **Beta 4** \`flip_rk817_charger.ko\` artifact (SHA-256 \`ee2b58290d49a62602435086c39d28bbd9c4fb0145fa81034dab413780050bb4\`, expected Linux \`5.10.160\`, vermagic \`5.10.160 SMP mod_unload aarch64\`). The contents and actual identity of the module **loaded on the physical device remain unverified**. Do not assume that the present C source was compiled into that binary.

The **RK817 datasheet V2.1**, page 41, says the PMIC can pause charging based on the TS battery thermistor; page 124 defines \`PMIC_CHRG_OUT 0xE4\`, including current-selection bits [2:0] and power-path/charging voltage/foldback bits in the same byte. **The datasheet does not prove the Miyoo Flip has wired/configured the TS protection correctly.** A 500 mA selector is a current setting, not software charging-disable.

Newer upstream Linux \`drivers/power/supply/rk817_charger.c\` uses the parent RK817 regmap and documents PMIC charging status. Its Kconfig support is for 6.1 and newer mainline kernels, and **must not be treated as the exact vendor 5.10 driver**:
- https://codebrowser.dev/linux/linux/drivers/power/supply/rk817_charger.c.html
- https://cateee.net/lkddb/web-lkddb/CHARGER_RK817.html

## Added read-only utility (NOT packaged/deployed)

\`tools/diagnostics/flip_charge_observe.py\` is a manually invoked **developer-side/physical-test preparation** utility. It reads only fixed fields from:
- battery power_supply: \`temp\` (tenths °C), \`status\`, \`voltage_now\` (µV), \`current_now\` (µA), \`capacity\` (%)
- AC/USB \`online\`, and the charger helper's *requested* \`current_limit_ma\`
- \`/tmp/flipos-charge-service.pid\` and \`/proc/PID/exe\`, compared by device/inode against the expected service binary

Output is bounded JSON lines written to **stdout only**. No ROM titles, serial numbers, Wi-Fi, developer temperature overrides or raw logs are included. It never executes charger/PMIC writes, disables/enables charging, starts/kills a daemon, or uploads information. The health field is **observation only**:
- \`confirmed_process_identity\`: executable identity matches the expected file at observation time; does not guarantee healthy supervision or actual charger operation.
- \`pid_not_running\`: PID's process directory absent.
- \`different_or_replaced_executable\`: PID refers to another executable or an older executable since binary replacement. Do **not** conclude it is safe to launch a second daemon.
- \`identity_unavailable\`, \`invalid_pid_file\`, \`pid_file_unavailable\`: inconclusive; do not auto-restart.

Allowed range: 1–120 samples. For multiple live samples, sampling interval must be between **5 and 60 seconds**. Synthetic host fixtures support a zero-second interval. Nothing is stored on the SD card or sent over the network by the tool.

### Reproduce host tests (no Miyoo device)

\`\`\`sh
python3 -m unittest discover -s tests -p 'test_charge_observation.py' -v
make verify
\`\`\`

**Physical access required before collection.** The host tests only touch temporary fake \`/sys\`, \`/proc\`, and \`/tmp\` paths; they do not simulate a real PMIC.

## Proposed safe room-temperature charging-while-playing matrix

**Not performed.** Only after MSI source reconciliation, driver provenance review and explicit on-device authorisation:

| Scenario | Measurement | Required observation |
| --- | --- | --- |
| Idle unplugged | 5 minutes, 10-second read-only samples | Battery temp/current/voltage trend; verify game saves unaffected |
| Idle plugged in | 5 minutes, 10-second samples | Same readings; USB/AC status; no thermal or charging abnormality |
| GB and GBC gameplay unplugged | 5 minutes each | FPS/audio, battery temperature, battery drain |
| GBA demanding gameplay unplugged | 5 minutes | Same plus frame-pacing/CPU performance |
| GB/GBC/GBA gameplay plugged in | 5 minutes each | Correlate battery temp/status/current with log-level frame pacing |
| Lid suspend/awake with charger | Observe short, attended transitions | Charge service PID identity and status; no duplicate service |
| Safe disconnect/reconnect | Attended only, outside update/save transactions | USB/AC status transition, stock charger ownership and absence of errors |

Do **not** claim a PMIC current cap equals measured cell current or actual charge-enable state. Compare cold and warmed-up stable baseline conditions only at **ordinary room temperatures**. Never intentionally heat/chill the battery to trigger extremes, short power lines, bypass thermistor protection, write raw I²C registers, modify NAND/MTD or run unattended charge stress tests. Protect current game saves by doing game tests only after a disposable-save plan is approved.

## Service supervision design pending kernel ownership proof

1. Observe accurate identity and startup behaviour rather than accepting \`kill -0\` alone.
2. Determine the stock driver and \`flip_rk817_charger.ko\` ownership of the PMIC current selector.
3. If safe, design a **single-owner** charge service with an explicit bounded restart policy, process identity, a non-stale lock and an error state requiring manual/stock-safe fallback. Simply spawning another daemon when PID is uncertain can create contradictory writes.
4. Audit \`flip-charge-service\` SIGTERM behaviour: it currently calls \`flip_charger_restore\` on a clean exit. The source-level risk of restoring a stale higher selector remains **unresolved** pending a coordinated kernel fix.
5. Keep draft charging PRs #50–#52 and this one separate from `main`. Host CI cannot confirm stock driver thermal cutoff or charger safety.

## Evidence required to proceed

Exact MSI checkout/commit, clean proof of the installed \`.ko\` SHA and module vermagic, kernel/DT source or dump identifying stock RK817 charger ownership and TS thermistor setup, and read-only room-temperature battery traces. These cannot be gathered while MSI and the physical Miyoo Flip are unavailable.

**Release disposition: no charging safety sign-off.** No actual PMIC operation was performed.
