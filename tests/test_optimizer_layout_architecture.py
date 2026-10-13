#!/usr/bin/env python3
"""Pixel-layout guardrails for CPU optimizer screens."""

from pathlib import Path
import re

source = Path("src/frontend/flip_frontend.c").read_text()


def define_int(name: str) -> int:
    match = re.search(rf"^#define\s+{re.escape(name)}\s+(\d+)\s*$", source, re.M)
    assert match, name
    return int(match.group(1))


margin = define_int("OPT_UI_MARGIN_X")
content = define_int("OPT_UI_CONTENT_W")
summary_y = define_int("OPT_UI_SUMMARY_Y")
summary_h = define_int("OPT_UI_SUMMARY_H")
action_y = define_int("OPT_UI_ACTION_Y")
action_h = define_int("OPT_UI_ACTION_H")
action_gap = define_int("OPT_UI_ACTION_GAP")
message_y = define_int("OPT_UI_MESSAGE_Y")
footer_y = define_int("OPT_UI_FOOTER_Y")
results_y = define_int("OPT_RESULTS_PANEL_Y")
results_h = define_int("OPT_RESULTS_PANEL_H")

assert content == 640 - 2 * margin
assert summary_y + summary_h < action_y
assert action_y + 5 * action_h + 4 * action_gap < message_y
assert message_y + 7 < footer_y
assert footer_y + 7 <= 480
assert results_y + results_h < footer_y

results_start = source.index("static void draw_optimizer_results(")
optimizer_start = source.index("static void draw_optimizer(", results_start)
results = source[results_start:optimizer_start]
optimizer_end = source.index("static void refresh_play_stats(", optimizer_start)
optimizer = source[optimizer_start:optimizer_end]

for block in (results, optimizer):
    assert "OPT_UI_CONTENT_W" in block
    assert "UI_SCALE_" in block

actions_start = source.index("static const char *optimizer_actions[]")
actions_end = source.index("};", actions_start) + 2
actions = source[actions_start:actions_end]

for token in (
    '"RUN STABILITY TEST"',
    '"RESTORE DEFAULT SETTINGS"',
):
    assert token in actions, token

for token in (
    "OPT_UI_SUMMARY_Y",
    "OPT_UI_ACTION_Y",
    "fit_text_to_width(",
    '"OPTIMIZE AGAIN"',
    '"START STABILITY TEST?"',
):
    assert token in optimizer, token

for token in (
    "OPT_RESULTS_PANEL_Y",
    '"SAFETY MARGIN: %u mV"',
    '"CPU SPEED"',
    '"DEFAULT"',
    '"TUNED"',
):
    assert token in results, token

print("optimizer layout architecture tests passed")
