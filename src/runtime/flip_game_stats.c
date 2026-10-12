#define _POSIX_C_SOURCE 200809L

#include "flip_game_stats.h"
#include "flip_paths.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef FLIP_GAME_STATS_ROOT
#define FLIP_GAME_STATS_ROOT FLIP_STATE_DATA_ROOT "/playstats"
#endif

#define FLIP_GAME_STATS_VERSION 1

static int copy_string(
    char *dest,
    size_t dest_size,
    const char *source)
{
    if (!dest || dest_size == 0 || !source)
        return -1;

    size_t length = strlen(source);
    if (length >= dest_size)
        return -1;

    memcpy(dest, source, length + 1);
    return 0;
}

static unsigned long long add_saturating(
    unsigned long long a,
    unsigned long long b)
{
    if (ULLONG_MAX - a < b)
        return ULLONG_MAX;
    return a + b;
}

static unsigned long long stats_hash(
    const char *system_id,
    const char *rom_path)
{
    const unsigned long long offset = 1469598103934665603ULL;
    const unsigned long long prime = 1099511628211ULL;
    unsigned long long hash = offset;

    const unsigned char *p =
        (const unsigned char *)(system_id ? system_id : "");
    while (*p) {
        hash ^= (unsigned long long)*p++;
        hash *= prime;
    }

    hash ^= 0xffULL;
    hash *= prime;

    p = (const unsigned char *)(rom_path ? rom_path : "");
    while (*p) {
        hash ^= (unsigned long long)*p++;
        hash *= prime;
    }

    return hash;
}

static int ensure_directory_tree(const char *path)
{
    if (!path || !path[0])
        return -1;

    char work[PATH_MAX];
    if (copy_string(work, sizeof(work), path) < 0)
        return -1;

    for (char *p = work + 1; *p; ++p) {
        if (*p != '/')
            continue;

        *p = '\0';
        if (mkdir(work, 0777) < 0 && errno != EEXIST)
            return -1;
        *p = '/';
    }

    if (mkdir(work, 0777) < 0 && errno != EEXIST)
        return -1;

    return 0;
}

static int stats_path(
    char *out,
    size_t out_size,
    const char *system_id,
    const char *rom_path)
{
    if (!out || out_size == 0 ||
        !system_id || !system_id[0] ||
        !rom_path || !rom_path[0])
        return -1;

    int n = snprintf(
        out,
        out_size,
        "%s/%016llx",
        FLIP_GAME_STATS_ROOT,
        stats_hash(system_id, rom_path));

    return n > 0 && (size_t)n < out_size ? 0 : -1;
}

static int parse_stats_file(
    const char *path,
    flip_game_stats_t *stats)
{
    if (!path || !stats)
        return -1;

    FILE *f = fopen(path, "r");
    if (!f)
        return errno == ENOENT ? 0 : -1;

    flip_game_stats_t parsed;
    memset(&parsed, 0, sizeof(parsed));

    unsigned version = 0;
    char line[768];

    while (fgets(line, sizeof(line), f)) {
        char *value = NULL;

        if (sscanf(line, "version=%u", &version) == 1) {
            continue;
        } else if (strncmp(line, "system=", 7) == 0) {
            value = line + 7;
            value[strcspn(value, "\r\n")] = '\0';
            if (copy_string(
                    parsed.system,
                    sizeof(parsed.system),
                    value) < 0) {
                fclose(f);
                return -1;
            }
        } else if (strncmp(line, "rom=", 4) == 0) {
            value = line + 4;
            value[strcspn(value, "\r\n")] = '\0';
            if (copy_string(
                    parsed.rom,
                    sizeof(parsed.rom),
                    value) < 0) {
                fclose(f);
                return -1;
            }
        } else if (sscanf(
                       line,
                       "launches=%llu",
                       &parsed.launches) == 1) {
        } else if (sscanf(
                       line,
                       "total_play_seconds=%llu",
                       &parsed.total_play_seconds) == 1) {
        } else if (sscanf(
                       line,
                       "longest_session_seconds=%llu",
                       &parsed.longest_session_seconds) == 1) {
        } else if (sscanf(
                       line,
                       "last_session_seconds=%llu",
                       &parsed.last_session_seconds) == 1) {
        } else if (sscanf(
                       line,
                       "first_played_epoch=%lld",
                       &parsed.first_played_epoch) == 1) {
        } else {
            (void)sscanf(
                line,
                "last_played_epoch=%lld",
                &parsed.last_played_epoch);
        }
    }

    if (ferror(f)) {
        fclose(f);
        return -1;
    }

    fclose(f);

    if (version != FLIP_GAME_STATS_VERSION ||
        !parsed.system[0] ||
        !parsed.rom[0])
        return -1;

    parsed.valid = 1;
    *stats = parsed;
    return 1;
}

static int write_stats(
    const flip_game_stats_t *stats)
{
    if (!stats ||
        !stats->system[0] ||
        !stats->rom[0])
        return -1;

    if (ensure_directory_tree(FLIP_GAME_STATS_ROOT) < 0)
        return -1;

    char path[PATH_MAX];
    if (stats_path(
            path,
            sizeof(path),
            stats->system,
            stats->rom) < 0)
        return -1;

    char tmp[PATH_MAX];
    int n = snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    if (n <= 0 || (size_t)n >= sizeof(tmp))
        return -1;

    FILE *f = fopen(tmp, "w");
    if (!f)
        return -1;

    int ok =
        fprintf(
            f,
            "version=%u\n"
            "system=%s\n"
            "rom=%s\n"
            "launches=%llu\n"
            "total_play_seconds=%llu\n"
            "longest_session_seconds=%llu\n"
            "last_session_seconds=%llu\n"
            "first_played_epoch=%lld\n"
            "last_played_epoch=%lld\n",
            FLIP_GAME_STATS_VERSION,
            stats->system,
            stats->rom,
            stats->launches,
            stats->total_play_seconds,
            stats->longest_session_seconds,
            stats->last_session_seconds,
            stats->first_played_epoch,
            stats->last_played_epoch) >= 0 &&
        fflush(f) == 0 &&
        fsync(fileno(f)) == 0;

    if (fclose(f) != 0)
        ok = 0;

    if (!ok) {
        (void)unlink(tmp);
        return -1;
    }

    if (rename(tmp, path) < 0) {
        (void)unlink(tmp);
        return -1;
    }

    int dirfd = open(
        FLIP_GAME_STATS_ROOT,
        O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dirfd >= 0) {
        (void)fsync(dirfd);
        close(dirfd);
    }

    return 0;
}

static void init_stats(
    flip_game_stats_t *stats,
    const char *system_id,
    const char *rom_path)
{
    memset(stats, 0, sizeof(*stats));
    stats->valid = 1;
    (void)copy_string(
        stats->system,
        sizeof(stats->system),
        system_id);
    (void)copy_string(
        stats->rom,
        sizeof(stats->rom),
        rom_path);
}

int flip_game_stats_load(
    const char *system_id,
    const char *rom_path,
    flip_game_stats_t *stats)
{
    if (!system_id || !system_id[0] ||
        !rom_path || !rom_path[0] ||
        !stats)
        return -1;

    char path[PATH_MAX];
    if (stats_path(
            path,
            sizeof(path),
            system_id,
            rom_path) < 0)
        return -1;

    int result = parse_stats_file(path, stats);
    if (result <= 0)
        return result;

    if (strcmp(stats->system, system_id) != 0 ||
        strcmp(stats->rom, rom_path) != 0)
        return -1;

    return 1;
}

int flip_game_stats_record_launch(
    const char *system_id,
    const char *rom_path,
    long long started_epoch)
{
    flip_game_stats_t stats;
    int loaded =
        flip_game_stats_load(
            system_id,
            rom_path,
            &stats);

    if (loaded < 0)
        return -1;

    if (loaded == 0)
        init_stats(&stats, system_id, rom_path);

    stats.launches =
        add_saturating(stats.launches, 1ULL);

    if (stats.first_played_epoch <= 0)
        stats.first_played_epoch = started_epoch;

    stats.last_played_epoch = started_epoch;

    return write_stats(&stats);
}

int flip_game_stats_record_session(
    const char *system_id,
    const char *rom_path,
    unsigned long long active_seconds,
    long long ended_epoch)
{
    flip_game_stats_t stats;
    int loaded =
        flip_game_stats_load(
            system_id,
            rom_path,
            &stats);

    if (loaded < 0)
        return -1;

    if (loaded == 0)
        init_stats(&stats, system_id, rom_path);

    stats.total_play_seconds =
        add_saturating(
            stats.total_play_seconds,
            active_seconds);

    stats.last_session_seconds =
        active_seconds;

    if (active_seconds >
        stats.longest_session_seconds)
        stats.longest_session_seconds =
            active_seconds;

    if (stats.first_played_epoch <= 0)
        stats.first_played_epoch = ended_epoch;

    stats.last_played_epoch = ended_epoch;

    return write_stats(&stats);
}

static int stats_before(
    const flip_game_stats_t *a,
    const flip_game_stats_t *b)
{
    if (a->total_play_seconds !=
        b->total_play_seconds)
        return a->total_play_seconds >
            b->total_play_seconds;

    return a->last_played_epoch >
        b->last_played_epoch;
}

static void top_insert(
    flip_game_stats_t *top,
    size_t capacity,
    size_t *count,
    const flip_game_stats_t *candidate)
{
    if (!top || !count || capacity == 0 || !candidate)
        return;

    size_t used = *count;
    if (used > capacity)
        used = capacity;

    size_t pos = 0;
    while (pos < used &&
           !stats_before(candidate, &top[pos]))
        pos++;

    if (pos >= capacity)
        return;

    size_t new_used =
        used < capacity ? used + 1 : used;

    for (size_t i = new_used;
         i > pos + 1;
         --i) {
        top[i - 1] = top[i - 2];
    }

    top[pos] = *candidate;
    *count = new_used;
}

int flip_game_stats_collect(
    flip_game_stats_summary_t *summary,
    flip_game_stats_t *top_entries,
    size_t top_capacity,
    size_t *top_count)
{
    if (!summary)
        return -1;

    memset(summary, 0, sizeof(*summary));

    if (top_count)
        *top_count = 0;

    DIR *dir = opendir(FLIP_GAME_STATS_ROOT);
    if (!dir) {
        if (errno == ENOENT)
            return 0;
        return -1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.')
            continue;

        char path[PATH_MAX];
        int n = snprintf(
            path,
            sizeof(path),
            "%s/%s",
            FLIP_GAME_STATS_ROOT,
            entry->d_name);

        if (n <= 0 || (size_t)n >= sizeof(path))
            continue;

        flip_game_stats_t stats;
        if (parse_stats_file(path, &stats) != 1)
            continue;

        summary->games_tracked++;
        summary->total_launches =
            add_saturating(
                summary->total_launches,
                stats.launches);
        summary->total_play_seconds =
            add_saturating(
                summary->total_play_seconds,
                stats.total_play_seconds);

        if (stats.longest_session_seconds >
            summary->longest_session_seconds)
            summary->longest_session_seconds =
                stats.longest_session_seconds;

        if (top_entries && top_count && top_capacity > 0)
            top_insert(
                top_entries,
                top_capacity,
                top_count,
                &stats);
    }

    closedir(dir);
    return 0;
}
