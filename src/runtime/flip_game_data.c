#define _POSIX_C_SOURCE 200809L

#include "flip_game_data.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef FLIP_GAME_DATA_TEST_ROOT
#define FLIP_GAME_DATA_ROOT FLIP_GAME_DATA_TEST_ROOT
#else
#define FLIP_GAME_DATA_ROOT "/mnt/SDCARD/flipos-dev"
#endif

typedef struct {
    char active[1024];
    char quarantine[1024];
    int moved;
} flip_game_data_move_t;

static uint64_t game_data_hash(
    const char *text)
{
    uint64_t hash =
        UINT64_C(14695981039346656037);

    while (text && *text) {
        hash ^= (unsigned char)*text++;
        hash *= UINT64_C(1099511628211);
    }

    return hash;
}

static int valid_system_id(
    const char *system_id)
{
    if (!system_id ||
        !system_id[0])
        return 0;

    for (const char *p = system_id;
         *p;
         ++p) {

        unsigned char c =
            (unsigned char)*p;

        if (!(
                (c >= 'A' && c <= 'Z') ||
                (c >= 'a' && c <= 'z') ||
                (c >= '0' && c <= '9') ||
                c == '_' ||
                c == '-')) {

            return 0;
        }
    }

    return 1;
}

static int ensure_reset_directory(void)
{
    char path[1024];

    int written = snprintf(
        path,
        sizeof(path),
        "%s/reset",
        FLIP_GAME_DATA_ROOT);

    if (written < 0 ||
        (size_t)written >= sizeof(path))
        return -1;

    if (mkdir(path, 0777) < 0 &&
        errno != EEXIST) {

        return -1;
    }

    return 0;
}

static int build_move(
    flip_game_data_move_t *move,
    const char *system_id,
    uint64_t hash,
    const char *category,
    const char *suffix)
{
    if (!move ||
        !system_id ||
        !category ||
        !suffix)
        return -1;

    int written = snprintf(
        move->active,
        sizeof(move->active),
        "%s/%s/%s/%016llx%s",
        FLIP_GAME_DATA_ROOT,
        category,
        system_id,
        (unsigned long long)hash,
        suffix);

    if (written < 0 ||
        (size_t)written >= sizeof(move->active))
        return -1;

    written = snprintf(
        move->quarantine,
        sizeof(move->quarantine),
        "%s/reset/%016llx%s",
        FLIP_GAME_DATA_ROOT,
        (unsigned long long)hash,
        suffix);

    if (written < 0 ||
        (size_t)written >= sizeof(move->quarantine))
        return -1;

    move->moved = 0;

    return 0;
}

static int regular_file_exists(
    const char *path)
{
    struct stat st;

    if (stat(path, &st) < 0) {

        if (errno == ENOENT)
            return 0;

        return -1;
    }

    if (!S_ISREG(st.st_mode))
        return -1;

    return 1;
}

static void rollback_moves(
    flip_game_data_move_t *moves,
    size_t count)
{
    while (count > 0) {
        --count;

        if (!moves[count].moved)
            continue;

        if (rename(
                moves[count].quarantine,
                moves[count].active) < 0) {

            fprintf(
                stderr,
                "FLIP_GAME_DATA: ROLLBACK_FAILED from=%s to=%s error=%s\n",
                moves[count].quarantine,
                moves[count].active,
                strerror(errno));
        }
    }
}

int flip_game_data_reset(
    const char *system_id,
    const char *persistence_system_id,
    const char *rom_path)
{
    if (!valid_system_id(system_id) ||
        !valid_system_id(persistence_system_id) ||
        !rom_path ||
        !rom_path[0]) {

        return -1;
    }

    if (ensure_reset_directory() < 0)
        return -1;

    uint64_t hash =
        game_data_hash(rom_path);

    flip_game_data_move_t moves[8];

    struct {
        const char *category;
        const char *suffix;
    } artifacts[] = {
        { "saves",    ".sav" },
        { "saves",    ".sav.prev" },
        { "saves",    ".sav.recovery" },
        { "states",   ".state" },
        { "states",   ".state.prev" },
        { "states",   ".state.recovery" },
        { "sessions", ".restore-pending" },
        { "sessions", ".session" },
    };

    size_t artifact_count =
        sizeof(artifacts) /
        sizeof(artifacts[0]);

    for (size_t i = 0;
         i < artifact_count;
         ++i) {

        const char *storage_system_id =
            strcmp(
                artifacts[i].category,
                "sessions") == 0
                ? system_id
                : persistence_system_id;

        if (build_move(
                &moves[i],
                storage_system_id,
                hash,
                artifacts[i].category,
                artifacts[i].suffix) < 0) {

            return -1;
        }
    }

    /*
     * Preflight all existing active artifacts before moving
     * anything. Never overwrite an earlier quarantine.
     */
    for (size_t i = 0;
         i < artifact_count;
         ++i) {

        int active =
            regular_file_exists(
                moves[i].active);

        if (active < 0)
            return -1;

        if (!active)
            continue;

        int quarantined =
            regular_file_exists(
                moves[i].quarantine);

        if (quarantined < 0)
            return -1;

        if (quarantined > 0) {
            fprintf(
                stderr,
                "FLIP_GAME_DATA: RESET_REFUSED quarantine-exists=%s\n",
                moves[i].quarantine);

            return -1;
        }
    }

    size_t moved_count = 0;

    for (size_t i = 0;
         i < artifact_count;
         ++i) {

        int active =
            regular_file_exists(
                moves[i].active);

        if (active < 0) {
            rollback_moves(
                moves,
                i);
            return -1;
        }

        if (!active)
            continue;

        if (rename(
                moves[i].active,
                moves[i].quarantine) < 0) {

            fprintf(
                stderr,
                "FLIP_GAME_DATA: RESET_FAILED from=%s to=%s error=%s\n",
                moves[i].active,
                moves[i].quarantine,
                strerror(errno));

            rollback_moves(
                moves,
                i);

            return -1;
        }

        moves[i].moved = 1;
        moved_count++;

#ifdef FLIP_GAME_DATA_TEST_FAIL_AFTER_MOVE
        if ((int)i + 1 ==
            FLIP_GAME_DATA_TEST_FAIL_AFTER_MOVE) {

            fprintf(
                stderr,
                "FLIP_GAME_DATA: TEST_FORCED_FAILURE after=%d\n",
                FLIP_GAME_DATA_TEST_FAIL_AFTER_MOVE);

            rollback_moves(
                moves,
                i + 1);

            return -1;
        }
#endif
    }

    if (moved_count == 0) {
        fprintf(
            stderr,
            "FLIP_GAME_DATA: RESET_NONE system=%s rom=%s\n",
            system_id,
            rom_path);

        return -1;
    }

    fprintf(
        stderr,
        "FLIP_GAME_DATA: RESET_OK system=%s rom=%s\n",
        system_id,
        rom_path);

    return 0;
}
