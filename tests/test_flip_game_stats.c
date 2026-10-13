#define _POSIX_C_SOURCE 200809L

#include "flip_game_stats.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ROOT "/tmp/flipos-game-stats-test"

static void reset_root(void)
{
    int rc = system("rm -rf " ROOT);
    assert(rc == 0);
}

int main(void)
{
    reset_root();

    flip_game_stats_t stats;
    assert(
        flip_game_stats_load(
            "gba",
            "/roms/Pokemon FireRed.gba",
            &stats) == 0);

    assert(
        flip_game_stats_record_launch(
            "gba",
            "/roms/Pokemon FireRed.gba",
            1000) == 0);
    assert(
        flip_game_stats_record_session(
            "gba",
            "/roms/Pokemon FireRed.gba",
            90,
            1090) == 0);

    assert(
        flip_game_stats_record_launch(
            "gba",
            "/roms/Pokemon FireRed.gba",
            2000) == 0);
    assert(
        flip_game_stats_record_session(
            "gba",
            "/roms/Pokemon FireRed.gba",
            300,
            2300) == 0);

    assert(
        flip_game_stats_load(
            "gba",
            "/roms/Pokemon FireRed.gba",
            &stats) == 1);

    assert(stats.valid == 1);
    assert(strcmp(stats.system, "gba") == 0);
    assert(stats.launches == 2);
    assert(stats.total_play_seconds == 390);
    assert(stats.longest_session_seconds == 300);
    assert(stats.last_session_seconds == 300);
    assert(stats.first_played_epoch == 1000);
    assert(stats.last_played_epoch == 2300);

    assert(
        flip_game_stats_record_launch(
            "gbc",
            "/roms/Pokemon Crystal.gbc",
            3000) == 0);
    assert(
        flip_game_stats_record_session(
            "gbc",
            "/roms/Pokemon Crystal.gbc",
            600,
            3600) == 0);

    flip_game_stats_summary_t summary;
    flip_game_stats_t top[4];
    size_t top_count = 0;

    assert(
        flip_game_stats_collect(
            &summary,
            top,
            4,
            &top_count) == 0);

    assert(summary.games_tracked == 2);
    assert(summary.total_launches == 3);
    assert(summary.total_play_seconds == 990);
    assert(summary.longest_session_seconds == 600);
    assert(top_count == 2);
    assert(strcmp(top[0].system, "gbc") == 0);
    assert(top[0].total_play_seconds == 600);
    assert(strcmp(top[1].system, "gba") == 0);

    reset_root();

    puts("flip_game_stats tests passed");
    return 0;
}
