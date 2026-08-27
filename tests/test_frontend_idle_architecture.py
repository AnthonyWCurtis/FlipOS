#!/usr/bin/env python3

from pathlib import Path

errors = []

header = Path(
    "include/flip_frontend_idle.h"
)

source = Path(
    "src/frontend/flip_frontend_idle.c"
)

present = Path(
    "src/frontend/flip_frontend_present.c"
).read_text()

main = Path(
    "src/frontend/flip_frontend.c"
).read_text()

gameswitcher = Path(
    "src/frontend/flip_gameswitcher_ui.c"
).read_text()

save_safety = Path(
    "src/frontend/flip_save_safety_launch.c"
).read_text()

#
# One shared idle/screensaver service owns the policy.
#
if not header.exists():
    errors.append(
        "global frontend idle header missing"
    )

if not source.exists():
    errors.append(
        "global frontend idle implementation missing"
    )

#
# The policy is fixed at 30 seconds for this milestone.
#
if source.exists() and header.exists():
    text = source.read_text()
    header_text = header.read_text()

    for token in [
        "flip_frontend_idle_update(",
        "flip_frontend_idle_active(",
        "flip_frontend_idle_consume_wake(",
    ]:
        if token not in text:
            errors.append(
                f"idle manager missing behavior token: {token}"
            )

    for token in [
        "FLIP_FRONTEND_IDLE_SECONDS",
        "30.0",
    ]:
        if token not in header_text:
            errors.append(
                f"idle manager missing policy token: {token}"
            )

#
# Screensaver drawing belongs in the shared frontend presentation
# path so every FlipOS-owned screen inherits it.
#
if "flip_frontend_idle" not in present:
    errors.append(
        "shared presenter does not own screensaver composition"
    )

#
# Input-owning frontend loops must feed activity into the same
# idle manager rather than maintaining private idle timers.
#
for name, text in [
    ("main", main),
    ("gameswitcher", gameswitcher),
    ("save_safety", save_safety),
]:
    if "flip_frontend_idle" not in text:
        errors.append(
            f"{name} does not participate in global idle handling"
        )

#
# Screensaver implementation must not manipulate hardware power
# during this first milestone.
#
if source.exists():
    text = source.read_text()

    for forbidden in [
        "flip_power_set_brightness(",
        "flip_power_sleep_until_lid_open(",
        "/sys/class/backlight",
        "RB_POWER_OFF",
    ]:
        if forbidden in text:
            errors.append(
                "screensaver crosses display/power boundary: "
                f"{forbidden}"
            )

if errors:
    for error in errors:
        print(error)

    raise SystemExit(1)

print("frontend global idle architecture passed")
