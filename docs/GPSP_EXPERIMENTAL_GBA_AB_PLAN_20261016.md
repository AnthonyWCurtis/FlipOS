# Experimental GBA A/B core trial: gpSP versus mGBA

**Status:** experimental, opt-in developer candidate; 16 October 2026.
**Not installed, benchmarked on Miyoo Flip or enabled in the normal release.**

## Why

FlipOS currently uses a statically linked **mGBA** Libretro core inside
`flip-gba`, and a shared, native FlipOS Core Host, GPU video path, audio host
and system/game session management. gpSP is a candidate faster GBA emulator
because it has an **ARM64 JIT/dynamic recompiler** on the RK3566.

This branch adds a second statically linked ARM64 binary,
`build/flip-gba-gpsp-test`, compiled against pinned gpSP Git commit
`5819380c2ffb0900219d700a382ee68c464ebb99` (GPL-2.0). The standard
`build/flip-gba` and all normal system/launcher mappings still run mGBA.
The gpSP build is explicitly excluded from `make all`, `make verify`
and every ordinary release/deploy package.

## Developer host build

```sh
# Requires aarch64-linux-gnu-gcc/g++, make, git and network access once
make gba-gpsp-experimental
file build/flip-gba-gpsp-test
# Check ARM64/AArch64 ELF architecture and that it is a static runner
aarch64-linux-gnu-readelf -h build/flip-gba-gpsp-test
```

The Makefile downloads *only when this target is requested*, checks gpSP's
exact upstream commit, builds with `platform=arm64 STATIC_LINKING=1`,
and links it to the existing native FlipOS runner. No RetroArch program is
installed. Verify licence/source redistribution obligations before packaging.

## Save, state and RTC protection

**Absolutely do not test using original in-progress saves.** The gpSP
experiment uses `storage_id = "gba-gpsp-test"`, so its save, savestate and
recovery namespaces are separate from production mGBA's `gba` namespace.

Current pinned gpSP exposes `RETRO_MEMORY_SAVE_RAM` and reports a
128 KiB region. That makes the common Libretro save interface plausible,
but does **not prove safe save migration or automatic RTC correctness**.
gpSP and mGBA savestates are incompatible. Do not rename/copy states between
cores, run Save Manager migration, or change default GBA registration.
For Pokémon RTC, persistence and wall-clock behaviour need focused physical
testing before recommending any core switch.

## Actual on-device A/B test (only when MSI/handheld available)

Prerequisites:
1. Reconcile the unpublished MSI checkout into a new worktree. Do not clean,
   reset, stash or overwrite its in-progress Save Manager/BIOS code.
2. Run the host tests, compile **both** binaries using the same compiler,
   and record Git commit/hash, kernel and perf profile.
3. Back up games' original SRAM saves/states outside the SD and use disposable
   ROM copies or test fixtures.
4. **Do not overwrite** the installed `flip-gba`; place the experiment into
   a separate path only with explicit device deployment consent. Run it under
   the standard FlipOS launch/runtime requirements, one emulator at a time.

Measure for Pokémon FireRed, Pokémon LeafGreen and Final Fantasy VI Advance:

| Metric | mGBA | gpSP | Rule |
|---|---|---|---|
| Core frame execution avg/max | Baseline | Candidate | Same scene/save/input and clock profile |
| Delivered FPS / dropped & late frames | Baseline | Candidate | ~59.7275 FPS target; no sustained regression |
| Audio underruns and glitches | Baseline | Candidate | No extra drops or clicking |
| CPU clock / CPU load | Baseline | Candidate | Compare like-for-like, not synthetic guessed FPS |
| Battery current / power and SoC temp | Baseline | Candidate | Unplugged controlled measurements only |
| GPU frame pipeline | Baseline | Candidate | Shared host, same screen settings |
| Gameplay/visual correctness | Baseline | Candidate | Check backgrounds, transparency and scrolling |
| Save/RTC + lid/resume | Production test copy | Candidate test copy | Never use production progress |

Use the standard `docs/PERFORMANCE_TESTING.md` guidelines:
same brightness and volume, 15 seconds warmup, 60-second capture windows,
and for battery comparisons repeat unplugged trials in alternating ABBA
order with a consistent battery/temperature starting range. Do not treat
USB-connected battery current as a battery-life estimate.

**Known caution:** gpSP has historically shown visual/tiling issues with
Final Fantasy VI Advance. That game must receive a visual compatibility
check; FPS alone is not a win. ARM64 JIT may also depend on working executable
memory mapping on the stock kernel.

## Conditions for enabling it in FlipOS

Only after gpSP passes save safety, startup/exit, RTC, sound, frame pacing,
sleep/resume, GPU display, compatibility and measured power tests should
we discuss exposing it as a per-game optional core. The default should
remain mGBA unless a clear, repeatable hardware advantage is found.
No core-auto-selection or state migration is in this prototype.

**Hardware acceptance: PENDING.** Host/CI compiler success does not prove a
real Miyoo Flip's dynamic recompiler or in-game speed.
