#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
frontend = (ROOT / "src/frontend/flip_frontend.c").read_text()
stats = (ROOT / "src/runtime/flip_game_stats.c").read_text()
header = (ROOT / "include/flip_game_stats.h").read_text()
session_h = (ROOT / "include/flip_session.h").read_text()
session = (ROOT / "src/runtime/flip_session.c").read_text()
makefile = (ROOT / "Makefile").read_text()

for token in (
    "flip_game_stats_record_launch",
    "flip_game_stats_record_session",
    "flip_game_stats_collect",
    "longest_session_seconds",
    "last_session_seconds",
    "first_played_epoch",
    "last_played_epoch",
):
    assert token in stats or token in header, token

for token in (
    '#include "flip_game_stats.h"',
    "SCREEN_STATS",
    '"PLAY STATS"',
    '"MOST PLAYED"',
    '"TOTAL PLAY TIME"',
    "refresh_play_stats",
    "flip_game_stats_record_launch(",
    "flip_game_stats_record_session(",
    "result.launched",
    "!automatic_resume",
    "result.active_seconds",
):
    assert token in frontend, token

for token in (
    "active_seconds",
    "CLOCK_MONOTONIC",
    "clock_gettime(",
):
    assert token in session_h or token in session, token

assert "$(BUILD)/flip_game_stats.o" in makefile
assert "test-game-stats" in makefile
assert "tests/test_play_stats_architecture.py" in makefile

print("play stats architecture passed")
