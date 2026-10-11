#!/usr/bin/env python3
from pathlib import Path

bridge = Path("src/emulators/flip_libretro_core.c").read_text()
gambatte = Path("src/emulators/flip_gambatte_core.c").read_text()

errors = []

for token in [
    "retro_get_memory_data(RETRO_MEMORY_SAVE_RAM)",
    "retro_get_memory_size(RETRO_MEMORY_SAVE_RAM)",
    ".save_memory_data =",
    ".save_memory_size =",
    "retro_get_memory_data(RETRO_MEMORY_RTC)",
    "retro_get_memory_size(RETRO_MEMORY_RTC)",
    ".rtc_memory_data =",
    ".rtc_memory_size =",
]:
    if token not in bridge:
        errors.append(f"shared Libretro battery-memory token missing: {token}")

for token in [
    'flip_libretro_core_adapter(&config)',
    '.bios_system_id = "GB"',
    '.storage_id = "gb"',
]:
    if token not in gambatte:
        errors.append(f"thin Gambatte adapter missing: {token}")

if errors:
    for error in errors:
        print(error)
    raise SystemExit(1)

print("shared Libretro save-memory architecture passed")

runner = Path("src/runtime/flip_emulator_runner.c").read_text()
for token in ("flip_rtc_load(core, context)", "flip_rtc_commit(core, context)"):
    if token not in runner:
        raise SystemExit(f"RTC persistence lifecycle token missing: {token}")

host = Path("src/runtime/flip_core_host.c").read_text()
host_header = Path("include/flip_core_host.h").read_text()

for token in (
    ".post_restore = reconcile_rtc_after_resume",
    ".post_restore_userdata = &save_context",
    "static int reconcile_rtc_after_resume(",
):
    if token not in runner:
        raise SystemExit(f"post-resume RTC reconciliation token missing: {token}")

for token in (
    "flip_core_host_state_fn post_restore;",
    "void *post_restore_userdata;",
):
    if token not in host_header:
        raise SystemExit(f"core-host post-restore contract missing: {token}")

restore_pos = host.find("restore_result = config->restore_state")
post_pos = host.find("config->post_restore(", restore_pos)
timing_pos = host.find("flip_lifecycle_resume_timing", restore_pos)
if restore_pos < 0 or post_pos < 0 or timing_pos < 0:
    raise SystemExit("core-host resume reconciliation path missing")
if not (restore_pos < post_pos < timing_pos):
    raise SystemExit("post-restore reconciliation must run after state restore and before resumed execution")
