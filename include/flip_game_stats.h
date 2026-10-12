#ifndef FLIP_GAME_STATS_H
#define FLIP_GAME_STATS_H

#include <stddef.h>

#define FLIP_GAME_STATS_SYSTEM_MAX 32
#define FLIP_GAME_STATS_ROM_MAX 512

typedef struct {
    int valid;
    char system[FLIP_GAME_STATS_SYSTEM_MAX];
    char rom[FLIP_GAME_STATS_ROM_MAX];

    unsigned long long launches;
    unsigned long long total_play_seconds;
    unsigned long long longest_session_seconds;
    unsigned long long last_session_seconds;

    long long first_played_epoch;
    long long last_played_epoch;
} flip_game_stats_t;

typedef struct {
    unsigned games_tracked;
    unsigned long long total_launches;
    unsigned long long total_play_seconds;
    unsigned long long longest_session_seconds;
} flip_game_stats_summary_t;

/*
 * Load stats for one ROM.
 *
 * Returns:
 *   1 = stats loaded
 *   0 = no stats yet
 *  -1 = malformed/read error
 */
int flip_game_stats_load(
    const char *system_id,
    const char *rom_path,
    flip_game_stats_t *stats);

/*
 * Record one user-initiated play session. Automatic lid-wake relaunches must
 * not call this again; they are continuations of the same user launch.
 */
int flip_game_stats_record_launch(
    const char *system_id,
    const char *rom_path,
    long long started_epoch);

/*
 * Add active emulator time for one completed user play session. Callers should
 * exclude time spent in lid sleep.
 */
int flip_game_stats_record_session(
    const char *system_id,
    const char *rom_path,
    unsigned long long active_seconds,
    long long ended_epoch);

/*
 * Collect the library-wide summary and the most-played games.
 *
 * top_entries is optional. When supplied, entries are sorted by total play
 * time descending, then by most-recently played.
 */
int flip_game_stats_collect(
    flip_game_stats_summary_t *summary,
    flip_game_stats_t *top_entries,
    size_t top_capacity,
    size_t *top_count);

#endif
