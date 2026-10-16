#!/usr/bin/env python3
"""Guard the experimental gpSP ARM64 path and stock mGBA isolation."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


def read(path):
    return (ROOT / path).read_text(encoding="utf-8")


class GpSPExperimentalArchitecture(unittest.TestCase):
    def test_source_pin_is_exact_and_disclosed(self):
        build = read("Makefile")
        pin = read("tools/gpsp/UPSTREAM_PIN.txt")
        sha = "5819380c2ffb0900219d700a382ee68c464ebb99"
        self.assertIn("GPSP_REF := " + sha, build)
        self.assertIn("Git commit: " + sha, pin)
        self.assertIn('git -C "$(GPSP_SOURCE)" rev-parse HEAD | grep -Fx "$(GPSP_REF)"', build)
        self.assertIn("STATIC_LINKING=1", build)
        self.assertIn("platform=arm64", build)

    def test_normal_gba_entrypoint_remains_mgba(self):
        launcher = read("src/emulators/flip_gba.c")
        system = read("src/runtime/flip_system.c")
        self.assertIn("flip_mgba_core_adapter()", launcher)
        self.assertIn('"gba"', launcher)
        self.assertIn('/mnt/SDCARD/flipos-dev/bin/flip-gba"', system)
        self.assertNotIn("gpsp", system.lower())
        self.assertIn("gba: $(BUILD)/flip-gba", read("Makefile"))

    def test_experimental_runner_separates_persistence(self):
        launcher = read("src/emulators/flip_gba_gpsp.c")
        core = read("src/emulators/flip_gpsp_core.c")
        self.assertIn('.storage_id = "gba-gpsp-test"', launcher)
        self.assertIn('.storage_id = "gba-gpsp-test"', core)
        self.assertIn('.bios_system_id = "GBA"', core)
        self.assertIn("flip_emulator_run(&config, argc, argv)", launcher)
        self.assertIn("flip_libretro_core_adapter(&config)", core)
        self.assertNotIn(".storage_id = \"gba\"", launcher)

    def test_is_opt_in_and_not_in_default_release(self):
        m = read("Makefile")
        self.assertIn("gba-gpsp-experimental: $(BUILD)/flip-gba-gpsp-test", m)
        self.assertIn("all: frontend gb gba mtp-responder", m)
        self.assertNotIn("gpsp", read("tools/update/build-release-package").lower())
        self.assertNotIn("gpsp", read("tools/update/build-beta-install-bundle").lower())
        self.assertNotIn("gpsp", read("tools/device/deploy-playtest").lower())
        self.assertNotIn("flip-gba-gpsp-test", read("src/runtime/flip_system.c"))
        for line in m.splitlines():
            if line.startswith("verify:") or line.startswith("gba:"):
                self.assertNotIn("gpsp", line)

    def test_libretro_savestate_core_is_separate_binary(self):
        m = read("Makefile")
        self.assertIn("$(BUILD)/flip-gba-gpsp-test: $(GPSP_OBJS) $(BUILD)/gpsp_libretro.a", m)
        self.assertIn("$(BUILD)/flip-gba: $(GBA_OBJS) $(BUILD)/mgba_libretro.a", m)
        self.assertNotIn("flip_mgba_core.o", re.search(
            r"GPSP_OBJS := \\\n(?:.*\\\n)+\s*\$\(EMULATOR_OBJS\)", m).group(0))

    def test_no_retroarch_dependency_added(self):
        files = ("src/emulators/flip_gba_gpsp.c", "src/emulators/flip_gpsp_core.c",
                 "tools/gpsp/UPSTREAM_PIN.txt")
        self.assertTrue(all("RetroArch" not in read(x) for x in files))


if __name__ == "__main__":
    unittest.main()
