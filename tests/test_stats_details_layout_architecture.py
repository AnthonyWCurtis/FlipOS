#!/usr/bin/env python3
"""Geometry guardrails for Play Stats and Game Details."""

from pathlib import Path
import re

source = Path("src/frontend/flip_frontend.c").read_text()


def define_int(name: str) -> int:
    match = re.search(rf"^#define\s+{re.escape(name)}\s+(\d+)\s*$", source, re.M)
    assert match, f"missing layout define {name}"
    return int(match.group(1))


stats_margin = define_int("STATS_MARGIN_X")
stats_w = define_int("STATS_CONTENT_W")
summary_y = define_int("STATS_SUMMARY_Y")
summary_h = define_int("STATS_SUMMARY_H")
summary_w = define_int("STATS_SUMMARY_W")
summary_gap = define_int("STATS_SUMMARY_GAP")
list_y = define_int("STATS_LIST_Y")
row_h = define_int("STATS_ROW_H")
row_gap = define_int("STATS_ROW_GAP")
stats_footer = define_int("STATS_FOOTER_Y")

assert stats_w == 640 - 2 * stats_margin
assert stats_margin + 3 * summary_w + 2 * summary_gap <= 640 - stats_margin
assert summary_y + summary_h < list_y
assert list_y + 4 * row_h + 3 * row_gap < stats_footer
assert stats_footer + 7 <= 480

details_margin = define_int("DETAILS_MARGIN_X")
details_w = define_int("DETAILS_CONTENT_W")
card_y = define_int("DETAILS_CARD_Y")
card_h = define_int("DETAILS_CARD_H")
card_w = define_int("DETAILS_CARD_W")
card_gap = define_int("DETAILS_CARD_GAP")
rows_y = define_int("DETAILS_ROWS_Y")
row_h = define_int("DETAILS_ROW_H")
details_footer = define_int("DETAILS_FOOTER_Y")

assert details_w == 640 - 2 * details_margin
assert details_margin + 2 * card_w + card_gap <= 640 - details_margin
assert card_y + card_h < rows_y
assert rows_y + 5 * row_h < 434
assert details_footer + 7 <= 480

stats = source[
    source.index("static void draw_stats("):
    source.index("static void draw_game_details(")
]
details = source[
    source.index("static void draw_game_details("):
    source.index("static void draw_systems(")
]

for block in (stats, details):
    assert "fit_text_to_width(" in block
    assert "centred_text_x_in_rect(" in block

assert "MOST PLAYED" in stats
assert "TOTAL PLAY TIME" in stats
assert "LAST SESSION" in details
assert "LONGEST SESSION" in details
assert "AVERAGE SESSION" in details
assert "FIRST PLAYED" in details
assert "LAST PLAYED" in details

print("stats/details layout architecture tests passed")
