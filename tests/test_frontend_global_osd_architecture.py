#!/usr/bin/env python3

from pathlib import Path

files = {
    "main": Path("src/frontend/flip_frontend.c").read_text(),
    "gameswitcher": Path(
        "src/frontend/flip_gameswitcher_ui.c"
    ).read_text(),
    "save_safety": Path(
        "src/frontend/flip_save_safety_launch.c"
    ).read_text(),
}

errors = []

#
# FlipOS-owned UI must share one final presentation boundary.
#
for name, text in files.items():
    if "flip_frontend_present(" not in text:
        errors.append(
            f"{name} does not use global frontend presentation"
        )

#
# Individual frontend screens must no longer decide whether to
# composite the OSD themselves.
#
for name, text in files.items():
    if "flip_osd_draw(" in text:
        errors.append(
            f"{name} still owns OSD composition"
        )

#
# Direct DRM presentation is forbidden in migrated frontend-owned UI.
# The shared presenter will be the only frontend location allowed to
# call flip_drm_present().
#
for name, text in files.items():
    if "flip_drm_present(" in text:
        errors.append(
            f"{name} still presents DRM directly"
        )

present_header = Path(
    "include/flip_frontend_present.h"
)

present_source = Path(
    "src/frontend/flip_frontend_present.c"
)

if not present_header.exists():
    errors.append(
        "global frontend presentation header missing"
    )

if not present_source.exists():
    errors.append(
        "global frontend presentation implementation missing"
    )

if errors:
    for error in errors:
        print(error)
    raise SystemExit(1)

print("frontend global OSD architecture passed")
