#define _GNU_SOURCE

#include "flip_drm.h"
#include "flip_draw.h"
#include "flip_input.h"
#include "flip_hotkeys.h"
#include "flip_osd.h"
#include "flip_session.h"
#include "flip_paths.h"
#include "flip_session_registry.h"
#include "flip_settings.h"
#include "flip_system.h"
#include "flip_library.h"
#include "flip_log.h"
#include "flip_power.h"
#include "flip_battery.h"
#include "flip_optimizer.h"

#include <stdint.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/reboot.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define BG       0x00101010
#define FG       0x00ffffff
#define DIM      0x00505050
#define PANEL    0x00202020

#define MAX_ROMS 256
#define ROWS_VISIBLE 9

#define POWEROFF_HOLD_SECONDS 1.5

typedef enum {
    SCREEN_HOME = 0,
    SCREEN_SYSTEMS,
    SCREEN_GAMES,
    SCREEN_RECENT,
    SCREEN_SETTINGS,
    SCREEN_OPTIMIZER
} screen_t;

typedef struct {
    char name[128];
    char display[128];
    char path[512];

    /*
     * Registry system that owns this ROM.
     *
     * Keeping ownership on the entry allows the frontend
     * library and GameSwitcher to contain ROMs belonging
     * to different emulator systems.
     */
    const flip_system_t *system;
} rom_entry_t;

static volatile sig_atomic_t running = 1;

static rom_entry_t roms[MAX_ROMS];
static int rom_count = 0;

/*
 * Systems currently containing at least one supported ROM.
 *
 * This list is rebuilt from the system registry and SD-card
 * contents rather than being permanently cached.
 */
#define MAX_AVAILABLE_SYSTEMS 32

static const flip_system_t *
    available_systems[MAX_AVAILABLE_SYSTEMS];

static int available_system_count = 0;
static int system_selected = 0;

static rom_entry_t recent_roms[10];
static int recent_count = 0;

static char suspended_rom_path[512];
static int suspended_available = 0;

typedef enum {
    GAMESWITCHER_CONFIRM_NONE = 0,
    GAMESWITCHER_CONFIRM_CLOSE,
    GAMESWITCHER_CONFIRM_REMOVE
} gameswitcher_confirm_t;

static gameswitcher_confirm_t
    gameswitcher_confirm =
        GAMESWITCHER_CONFIRM_NONE;

static char gameswitcher_confirm_system[32];
static char gameswitcher_confirm_rom[512];

static const char *home_items[] = {
    "GAMES",
    "GAMESWITCHER",
    "SETTINGS",
    "HARDWARE TEST"
};

#define HOME_ITEM_COUNT \
    ((int)(sizeof(home_items) / sizeof(home_items[0])))


static const char *settings_items[] = {
    "CPU OPTIMIZATION",
    "BACK"
};

#define SETTINGS_ITEM_COUNT \
    ((int)(sizeof(settings_items) / sizeof(settings_items[0])))

static const char *optimizer_actions[] = {
    "OPTIMIZE CPU",
    "VIEW RESULTS",
    "RESET OPTIMIZATION",
    "BACK"
};

#define OPTIMIZER_ACTION_COUNT \
    ((int)(sizeof(optimizer_actions) / sizeof(optimizer_actions[0])))

#define OPTIMIZER_CALIBRATION_PATH \
    FLIP_STATE_DATA_ROOT "/optimizer/cpu-calibration"
#define OPTIMIZER_UI_RETURN_PATH \
    FLIP_STATE_DATA_ROOT "/optimizer/ui-return"
#define OPTIMIZER_CALIBRATION_TOOL \
    FLIP_BIN_ROOT "/flip-optimizer-calibration"
#define OPTIMIZER_SESSION_TOOL \
    FLIP_BIN_ROOT "/flip-optimizer-session"
#define OPTIMIZER_RUNTIME_TOOL \
    FLIP_BIN_ROOT "/flip-optimizer-runtime"

static void stop_handler(int sig)
{
    (void)sig;
    running = 0;
}

static double monotonic_seconds(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (double)ts.tv_sec +
           (double)ts.tv_nsec / 1000000000.0;
}

static int copy_string(
    char *dest,
    size_t dest_size,
    const char *source)
{
    if (!dest ||
        dest_size == 0)
        return -1;

    if (!source) {
        dest[0] = '\0';
        return 0;
    }

    size_t length =
        strlen(source);

    if (length >= dest_size) {
        memcpy(
            dest,
            source,
            dest_size - 1);

        dest[dest_size - 1] =
            '\0';

        return 1;
    }

    memcpy(
        dest,
        source,
        length + 1);

    return 0;
}

static int has_system_extension(
    const flip_system_t *system,
    const char *name)
{
    if (!system ||
        !name ||
        !system->rom_extensions ||
        system->rom_extension_count <= 0)
        return 0;

    const char *dot =
        strrchr(
            name,
            '.');

    if (!dot)
        return 0;

    for (int i = 0;
         i < system->rom_extension_count;
         ++i) {

        const char *extension =
            system->rom_extensions[i];

        if (extension &&
            strcasecmp(
                dot,
                extension) == 0) {

            return 1;
        }
    }

    return 0;
}

static void make_display_name(
    const flip_system_t *system,
    char *out,
    size_t out_size,
    const char *filename)
{
    size_t pos = 0;

    for (size_t i = 0;
         filename[i] &&
         pos + 1 < out_size;
         ++i) {

        char c = filename[i];

        /*
         * Stop before a registered extension.
         */
        if (c == '.' &&
            has_system_extension(
                system,
                &filename[i])) {

            break;
        }

        c = (char)toupper((unsigned char)c);

        /*
         * Keep the display string friendly to the tiny
         * FlipOS bitmap font.
         */
        if ((c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == ' ' ||
            c == '-') {

            out[pos++] = c;
        } else {
            out[pos++] = ' ';
        }
    }

    out[pos] = '\0';
}

static int compare_roms(
    const void *a,
    const void *b)
{
    const rom_entry_t *ra =
        (const rom_entry_t *)a;

    const rom_entry_t *rb =
        (const rom_entry_t *)b;

    return strcasecmp(
        ra->display,
        rb->display);
}

static int load_suspended_session(void)
{
    suspended_available = 0;
    suspended_rom_path[0] = '\0';

    flip_session_registry_entry_t session;

    int read_result =
        flip_session_registry_find_state(
            "suspended",
            &session);

    if (read_result <= 0)
        return read_result;

    const flip_system_t *session_system =
        flip_system_find(
            session.system);

    if (!session_system)
        return 0;

    if (access(
            session.rom,
            R_OK) != 0 ||
        !flip_system_supports_path(
            session_system,
            session.rom)) {

        return 0;
    }

    copy_string(
        suspended_rom_path,
        sizeof(suspended_rom_path),
        session.rom);

    suspended_available = 1;

    flip_log_write(
        FLIP_LOG_INFO,
        "FRONTEND_SESSION",
        "suspended registry rom=%s",
        session.rom);

    return 1;
}

static int recent_session_registry_state(
    const rom_entry_t *rom,
    char *state,
    size_t state_size)
{
    if (!state ||
        state_size == 0)
        return -1;

    state[0] = '\0';

    if (!rom ||
        !rom->system ||
        !rom->system->id ||
        !rom->path[0])
        return 0;

    flip_session_registry_entry_t session;

    int find_result =
        flip_session_registry_find(
            rom->system->id,
            rom->path,
            &session);

    if (find_result <= 0)
        return find_result;

    copy_string(
        state,
        state_size,
        session.state);

    return 1;
}

static int recent_session_is_recoverable(
    const rom_entry_t *rom)
{
    char state[32];

    if (recent_session_registry_state(
            rom,
            state,
            sizeof(state)) <= 0)
        return 0;

    return strcmp(state, "suspended") == 0 ||
           strcmp(state, "interrupted") == 0 ||
           strcmp(state, "signaled") == 0 ||
           strcmp(state, "error") == 0;
}

static int recent_index_for_path(
    const char *path)
{
    if (!path ||
        !path[0])
        return -1;

    for (int i = 0;
         i < recent_count;
         ++i) {

        if (strcmp(
                recent_roms[i].path,
                path) == 0)
            return i;
    }

    return -1;
}

static void select_suspended_recent(
    int *selected)
{
    if (!selected)
        return;

    load_suspended_session();

    if (!suspended_available)
        return;

    int index =
        recent_index_for_path(
            suspended_rom_path);

    if (index >= 0)
        *selected = index;
}

static int load_recent_history(void)
{
    if (flip_session_prune_recent() < 0) {

        fprintf(
            stderr,
            "FRONTEND_RECENT history prune failed\n");
    }

    const char *history_file =
        FLIP_RECENT_FILE;

    recent_count = 0;

    FILE *f =
        fopen(
            history_file,
            "r");

    if (!f) {

        /*
         * Backward compatibility with RECENT v1:
         * use valid current-session metadata if no
         * dedicated history exists yet.
         */
        flip_session_state_t session;

        int read_result =
            flip_session_read_current(
                &session);

        if (read_result <= 0) {

            fprintf(
                stderr,
                "FRONTEND_RECENT no history\n");

            return 0;
        }

        const flip_system_t *recent_system =
            flip_system_find(
                session.system);

        if (!recent_system ||
            access(
                session.rom,
                R_OK) != 0 ||
            !flip_system_supports_path(
                recent_system,
                session.rom)) {

            return 0;
        }

        const char *base =
            strrchr(
                session.rom,
                '/');

        base =
            base
                ? base + 1
                : session.rom;

        copy_string(
            recent_roms[0].name,
            sizeof(recent_roms[0].name),
            base);

        copy_string(
            recent_roms[0].path,
            sizeof(recent_roms[0].path),
            session.rom);

        make_display_name(
            recent_system,
            recent_roms[0].display,
            sizeof(recent_roms[0].display),
            base);

        recent_roms[0].system =
            recent_system;

        recent_count = 1;

        fprintf(
            stderr,
            "FRONTEND_RECENT count=1 fallback=%s\n",
            session.rom);

        return 1;
    }

    char line[640];

    while (fgets(
               line,
               sizeof(line),
               f) &&
           recent_count < 10) {

        char *newline =
            strchr(line, '\n');

        if (newline)
            *newline = '\0';

        char *sep =
            strchr(line, '|');

        if (!sep)
            continue;

        *sep = '\0';

        const char *system =
            line;

        const char *rom_path =
            sep + 1;

        /*
         * Resolve the history entry through the current
         * FlipOS system registry.
         */
        const flip_system_t *recent_system =
            flip_system_find(
                system);

        if (!recent_system)
            continue;

        /*
         * Silently drop entries whose ROM disappeared or is
         * no longer a supported content type for this system.
         *
         * This keeps stale archive/history entries from
         * appearing as broken GameSwitcher sessions.
         */
        if (access(
                rom_path,
                R_OK) != 0 ||
            !has_system_extension(
                recent_system,
                rom_path)) {

            continue;
        }

        const char *base =
            strrchr(
                rom_path,
                '/');

        base =
            base ? base + 1 : rom_path;

        rom_entry_t *rom =
            &recent_roms[recent_count];

        snprintf(
            rom->name,
            sizeof(rom->name),
            "%s",
            base);

        snprintf(
            rom->path,
            sizeof(rom->path),
            "%s",
            rom_path);

        make_display_name(
            recent_system,
            rom->display,
            sizeof(rom->display),
            base);

        rom->system =
            recent_system;

        recent_count++;
    }

    fclose(f);

    fprintf(
        stderr,
        "FRONTEND_RECENT count=%d\n",
        recent_count);

    for (int i = 0;
         i < recent_count;
         ++i) {

        fprintf(
            stderr,
            "FRONTEND_RECENT index=%d rom=%s\n",
            i,
            recent_roms[i].path);
    }

    return recent_count;
}

/*
 * Return non-zero when a registered system currently has
 * at least one supported ROM on the SD card.
 *
 * This deliberately stops at the first match so rebuilding
 * the Systems screen remains inexpensive.
 */

static int system_has_roms(
    const flip_system_t *system)
{
    if (!system ||
        !system->rom_directory)
        return 0;

    DIR *dir =
        opendir(
            system->rom_directory);

    if (!dir)
        return 0;

    int found = 0;

    struct dirent *entry;

    while ((entry = readdir(dir))) {

        if (entry->d_name[0] == '.')
            continue;

        char path[512];

        int written =
            snprintf(
                path,
                sizeof(path),
                "%s/%s",
                system->rom_directory,
                entry->d_name);

        if (written < 0 ||
            (size_t)written >= sizeof(path))
            continue;

        if (flip_system_supports_path(
                system,
                path)) {

            found = 1;
            break;
        }
    }

    closedir(dir);

    return found;
}

/*
 * Rebuild the console list from the live system registry
 * and current SD-card contents.
 *
 * Empty systems are intentionally hidden.
 */
static void refresh_available_systems(void)
{
    available_system_count = 0;

    int count =
        flip_system_count();

    for (int i = 0;
         i < count &&
         available_system_count <
             MAX_AVAILABLE_SYSTEMS;
         ++i) {

        const flip_system_t *system =
            flip_system_at(i);

        if (!system)
            continue;

        flip_library_cache_status_t cache_status;

        if (flip_library_cache_evaluate(
                system,
                &cache_status) < 0) {

            fprintf(
                stderr,
                "LIBRARY_CACHE: update failed system=%s\n",
                system->id
                    ? system->id
                    : "?");
        }

        if (!system_has_roms(system))
            continue;

        available_systems[
            available_system_count++] =
                system;

        fprintf(
            stderr,
            "FRONTEND_SYSTEM available=%s\n",
            system->id
                ? system->id
                : "?");
    }

    if (system_selected >=
        available_system_count) {

        system_selected =
            available_system_count > 0
                ? available_system_count - 1
                : 0;
    }

    fprintf(
        stderr,
        "FRONTEND_SYSTEM count=%d\n",
        available_system_count);
}


static int load_system_roms_from_index(
    const flip_system_t *system)
{
    if (!system ||
        !system->rom_directory)
        return -1;

    /*
     * The cache stores filenames only. Everything else is
     * reconstructed from the current system registry so
     * frontend state cannot become detached from registry
     * configuration.
     */
    char names[MAX_ROMS][128];

    int count =
        flip_library_load_index(
            system,
            (char *)names,
            MAX_ROMS,
            sizeof(names[0]));

    if (count < 0)
        return -1;

    rom_count = 0;

    for (int i = 0;
         i < count;
         ++i) {

        const char *name =
            names[i];

        if (!has_system_extension(
                system,
                name)) {

            fprintf(
                stderr,
                "LIBRARY_CACHE: invalid cached extension system=%s name=%s\n",
                system->id
                    ? system->id
                    : "?",
                name);

            rom_count = 0;
            return -1;
        }

        rom_entry_t *rom =
            &roms[rom_count];

        copy_string(
            rom->name,
            sizeof(rom->name),
            name);

        int written =
            snprintf(
                rom->path,
                sizeof(rom->path),
                "%s/%s",
                system->rom_directory,
                name);

        if (written < 0 ||
            (size_t)written >=
                sizeof(rom->path)) {

            rom_count = 0;
            return -1;
        }

        /*
         * A fingerprint HIT already proves the live directory
         * matches this cache generation, but retain this cheap
         * safety check before exposing the entry to the UI.
         */
        if (access(
                rom->path,
                R_OK) != 0) {

            fprintf(
                stderr,
                "LIBRARY_CACHE: cached ROM missing path=%s\n",
                rom->path);

            rom_count = 0;
            return -1;
        }

        make_display_name(
            system,
            rom->display,
            sizeof(rom->display),
            name);

        rom->system =
            system;

        rom_count++;
    }

    fprintf(
        stderr,
        "LIBRARY_CACHE: index loaded system=%s roms=%d\n",
        system->id
            ? system->id
            : "?",
        rom_count);

    fprintf(
        stderr,
        "FRONTEND_ROMS count=%d source=cache\n",
        rom_count);

    return rom_count;
}

static int rebuild_system_roms(
    const flip_system_t *system)
{
    if (!system ||
        !system->rom_directory) {

        fprintf(
            stderr,
            "FRONTEND_ROMS invalid system\n");

        rom_count = 0;
        return -1;
    }

    const char *directory =
        system->rom_directory;

    DIR *dir =
        opendir(directory);

    rom_count = 0;

    if (!dir) {
        fprintf(
            stderr,
            "FRONTEND_ROMS opendir failed path=%s error=%s\n",
            directory,
            strerror(errno));

        return -1;
    }

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {

        if (entry->d_name[0] == '.')
            continue;

        if (!has_system_extension(
                system,
                entry->d_name))
            continue;

        if (rom_count >= MAX_ROMS)
            break;

        rom_entry_t *rom =
            &roms[rom_count];

        copy_string(
            rom->name,
            sizeof(rom->name),
            entry->d_name);

        snprintf(
            rom->path,
            sizeof(rom->path),
            "%s/%s",
            directory,
            entry->d_name);

        make_display_name(
            system,
            rom->display,
            sizeof(rom->display),
            entry->d_name);

        rom->system =
            system;

        rom_count++;
    }

    closedir(dir);

    qsort(
        roms,
        rom_count,
        sizeof(roms[0]),
        compare_roms);

    /*
     * Persist the exact sorted ROM order used by the
     * frontend. The cache stores filenames only; path,
     * display name and system ownership are reconstructed
     * when the index is loaded.
     */
    const char *index_names[MAX_ROMS];

    for (int i = 0;
         i < rom_count;
         ++i) {

        index_names[i] =
            roms[i].name;
    }

    if (flip_library_commit_generation(
            system,
            index_names,
            rom_count) < 0) {

        fprintf(
            stderr,
            "LIBRARY_CACHE: generation commit failed system=%s\n",
            system->id
                ? system->id
                : "?");
    }

    fprintf(
        stderr,
        "FRONTEND_ROMS count=%d\n",
        rom_count);

    for (int i = 0; i < rom_count; ++i) {
        fprintf(
            stderr,
            "FRONTEND_ROM index=%d path=%s\n",
            i,
            roms[i].path);
    }

    return rom_count;
}


static int load_system_roms(
    flip_drm_t *drm,
    const flip_system_t *system)
{
    if (!system)
        return -1;

    flip_library_cache_status_t cache_status;

    int cache_result =
        flip_library_cache_evaluate(
            system,
            &cache_status);

    /*
     * Complete cache-generation HIT:
     * try the cached index first.
     */
    if (cache_result == 0 &&
        cache_status.state ==
            FLIP_LIBRARY_CACHE_HIT) {

        int loaded =
            load_system_roms_from_index(
                system);

        if (loaded >= 0) {

            fprintf(
                stderr,
                "LIBRARY_CACHE: USE_INDEX system=%s roms=%d\n",
                system->id
                    ? system->id
                    : "?",
                loaded);

            return loaded;
        }

        /*
         * Fingerprint and index form one logical cache
         * generation. If the index is unavailable/corrupt,
         * rebuild the whole generation from the filesystem.
         */
        fprintf(
            stderr,
            "LIBRARY_CACHE: index invalid system=%s rebuilding\n",
            system->id
                ? system->id
                : "?");
    }

    if (cache_result == 0 &&
        cache_status.state !=
            FLIP_LIBRARY_CACHE_HIT) {

        fprintf(
            stderr,
            "LIBRARY_CACHE: rebuild required system=%s\n",
            system->id
                ? system->id
                : "?");

    } else if (cache_result < 0) {

        fprintf(
            stderr,
            "LIBRARY_CACHE: fingerprint unavailable system=%s rebuilding\n",
            system->id
                ? system->id
                : "?");
    }

    if (drm) {

        flip_osd_show_status(
            "UPDATING LIBRARY");

        /*
         * Present the status immediately before synchronous
         * filesystem work begins, otherwise a fast rebuild
         * could complete before the user ever sees it.
         */
        flip_osd_draw(
            drm);

        flip_drm_present(
            drm);
    }

    int result =
        rebuild_system_roms(
            system);

    if (result >= 0) {

        flip_osd_show_status(
            "LIBRARY UPDATED");

        fprintf(
            stderr,
            "LIBRARY_CACHE: OSD updated system=%s roms=%d\n",
            system->id
                ? system->id
                : "?",
            result);
    }

    return result;
}

static void power_off_now(void)
{
    fprintf(
        stderr,
        "FRONTEND: poweroff requested\n");

    fflush(stderr);

    sync();

    reboot(RB_POWER_OFF);

    perror(
        "FRONTEND: reboot(RB_POWER_OFF)");
}

static int centred_text_x(
    const char *text,
    int scale)
{
    if (!text || scale <= 0)
        return 0;

    /*
     * flip_draw_text advances 6 * scale pixels
     * for every character.
     */
    int width =
        (int)strlen(text) *
        6 *
        scale;

    return (640 - width) / 2;
}

static int read_int_file(
    const char *path,
    int *value)
{
    if (!path || !value)
        return -1;

    FILE *f = fopen(path, "r");
    if (!f)
        return -1;

    int parsed = 0;
    int rc = fscanf(f, "%d", &parsed);
    fclose(f);

    if (rc != 1)
        return -1;

    *value = parsed;
    return 0;
}

static int optimizer_external_power_online(void)
{
    int online = 0;

    if (read_int_file(
            "/sys/class/power_supply/usb/online",
            &online) == 0 &&
        online > 0)
        return 1;

    online = 0;
    if (read_int_file(
            "/sys/class/power_supply/ac/online",
            &online) == 0 &&
        online > 0)
        return 1;

    return 0;
}

static int optimizer_battery_percent(void)
{
    flip_battery_snapshot_t battery;

    if (flip_battery_read(&battery) < 0 ||
        !battery.percent_valid)
        return -1;

    return battery.percent;
}

static int optimizer_result_load(
    flip_optimizer_result_t *result)
{
    if (!result)
        return 0;

    return flip_optimizer_load(result) == 0
        ? 1
        : 0;
}

static int optimizer_run_tool(
    const char *tool,
    const char *arg1,
    const char *arg2)
{
    pid_t pid = fork();

    if (pid < 0)
        return -1;

    if (pid == 0) {
        if (arg2) {
            execl(
                tool,
                tool,
                arg1,
                arg2,
                (char *)NULL);
        } else if (arg1) {
            execl(
                tool,
                tool,
                arg1,
                (char *)NULL);
        } else {
            execl(
                tool,
                tool,
                (char *)NULL);
        }

        _exit(127);
    }

    int status = 0;

    do {
        if (waitpid(pid, &status, 0) < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        break;
    } while (1);

    if (!WIFEXITED(status) ||
        WEXITSTATUS(status) != 0)
        return -1;

    return 0;
}

static int optimizer_write_return_marker(void)
{
    FILE *f = fopen(
        OPTIMIZER_UI_RETURN_PATH,
        "w");

    if (!f)
        return -1;

    int failed =
        fprintf(
            f,
            "return_to_optimizer=1\n") < 0;

    if (fflush(f) != 0 ||
        fsync(fileno(f)) != 0 ||
        fclose(f) != 0)
        failed = 1;

    return failed ? -1 : 0;
}

static int optimizer_start_fresh(void)
{
    int battery = optimizer_battery_percent();

    if (!optimizer_external_power_online() ||
        battery < 30)
        return 1;

    if (optimizer_run_tool(
            OPTIMIZER_SESSION_TOOL,
            "stop",
            NULL) < 0)
        return -1;

    if (optimizer_run_tool(
            OPTIMIZER_CALIBRATION_TOOL,
            "snapshot",
            NULL) < 0)
        return -1;

    if (optimizer_run_tool(
            OPTIMIZER_CALIBRATION_TOOL,
            "reset-for-rerun",
            NULL) < 0) {
        (void)optimizer_run_tool(
            OPTIMIZER_CALIBRATION_TOOL,
            "restore-snapshot",
            NULL);
        return -1;
    }

    if (optimizer_run_tool(
            OPTIMIZER_CALIBRATION_TOOL,
            "init",
            NULL) < 0) {
        (void)optimizer_run_tool(
            OPTIMIZER_CALIBRATION_TOOL,
            "restore-snapshot",
            NULL);
        return -1;
    }

    if (optimizer_write_return_marker() < 0) {
        (void)optimizer_run_tool(
            OPTIMIZER_CALIBRATION_TOOL,
            "restore-snapshot",
            NULL);
        return -1;
    }

    if (optimizer_run_tool(
            OPTIMIZER_SESSION_TOOL,
            "start",
            "20") < 0) {
        (void)unlink(
            OPTIMIZER_UI_RETURN_PATH);
        (void)optimizer_run_tool(
            OPTIMIZER_CALIBRATION_TOOL,
            "restore-snapshot",
            NULL);
        return -1;
    }

    sync();
    return 0;
}

static int optimizer_reset_all(void)
{
    if (optimizer_run_tool(
            OPTIMIZER_SESSION_TOOL,
            "stop",
            NULL) < 0)
        return -1;

    if (optimizer_run_tool(
            OPTIMIZER_CALIBRATION_TOOL,
            "reset",
            NULL) < 0)
        return -1;

    (void)unlink(
        OPTIMIZER_UI_RETURN_PATH);

    sync();
    return 0;
}

static void draw_settings(
    flip_drm_t *drm,
    int selected,
    double poweroff_progress)
{
    flip_draw_clear(drm, BG);

    flip_draw_text(
        drm,
        centred_text_x("SETTINGS", 4),
        28,
        "SETTINGS",
        4,
        FG);

    flip_draw_text(
        drm,
        centred_text_x("SYSTEM", 2),
        76,
        "SYSTEM",
        2,
        DIM);

    for (int i = 0;
         i < SETTINGS_ITEM_COUNT;
         ++i) {

        int y = 155 + i * 62;

        if (i == selected) {
            flip_draw_rect(
                drm,
                125,
                y - 10,
                390,
                44,
                PANEL);
            flip_draw_rect_outline(
                drm,
                125,
                y - 10,
                390,
                44,
                2,
                FG);
        }

        flip_draw_text(
            drm,
            centred_text_x(
                settings_items[i],
                3),
            y,
            settings_items[i],
            3,
            i == selected
                ? FG
                : DIM);
    }

    flip_draw_text(
        drm,
        centred_text_x(
            "A SELECT   B BACK",
            1),
        452,
        "A SELECT   B BACK",
        1,
        DIM);

    flip_osd_set_poweroff_progress(
        poweroff_progress);
    flip_osd_draw(drm);
    flip_drm_present(drm);
}

static void draw_optimizer(
    flip_drm_t *drm,
    int selected,
    int confirm_action,
    const char *message,
    double poweroff_progress)
{
    flip_optimizer_result_t result;
    int have_result =
        optimizer_result_load(&result);

    int plugged =
        optimizer_external_power_online();

    int battery =
        optimizer_battery_percent();

    flip_draw_clear(drm, BG);

    flip_draw_text(
        drm,
        centred_text_x(
            "CPU OPTIMIZATION",
            3),
        20,
        "CPU OPTIMIZATION",
        3,
        FG);

    const char *state_text =
        have_result
            ? "OPTIMIZED"
            : "NOT OPTIMIZED";

    flip_draw_text(
        drm,
        centred_text_x(
            state_text,
            2),
        61,
        state_text,
        2,
        have_result
            ? FG
            : DIM);

    char power_text[96];
    if (battery >= 0) {
        snprintf(
            power_text,
            sizeof(power_text),
            "%s   BATTERY %d%%",
            plugged
                ? "CHARGER CONNECTED"
                : "CONNECT CHARGER",
            battery);
    } else {
        snprintf(
            power_text,
            sizeof(power_text),
            "%s",
            plugged
                ? "CHARGER CONNECTED"
                : "CONNECT CHARGER");
    }

    flip_draw_text(
        drm,
        centred_text_x(
            power_text,
            1),
        91,
        power_text,
        1,
        plugged
            ? FG
            : DIM);

    if (have_result) {
        flip_draw_text(
            drm,
            72,
            118,
            "STOCK -> OPTIMIZED",
            1,
            DIM);

        int shown = 0;
        for (size_t reverse = result.opp_count;
             reverse > 0 &&
             shown < 5;
             --reverse) {
            size_t i = reverse - 1;
            const flip_optimizer_opp_t *opp =
                &result.opps[i];

            char row[96];

            snprintf(
                row,
                sizeof(row),
                "%4u MHz   %4u -> %4u mV",
                opp->frequency_khz / 1000,
                opp->stock_uv / 1000,
                opp->optimized_uv / 1000);

            flip_draw_text(
                drm,
                72,
                137 + shown * 20,
                row,
                1,
                FG);

            shown++;
        }

        flip_draw_text(
            drm,
            72,
            241,
            "816 / 600 / 408 MHz kept at stock",
            1,
            DIM);
    } else {
        flip_draw_text(
            drm,
            centred_text_x(
                "DEVICE-SPECIFIC CPU VOLTAGE CALIBRATION",
                1),
            132,
            "DEVICE-SPECIFIC CPU VOLTAGE CALIBRATION",
            1,
            DIM);

        flip_draw_text(
            drm,
            centred_text_x(
                "SEVERAL AUTOMATIC RESTARTS ARE NORMAL",
                1),
            154,
            "SEVERAL AUTOMATIC RESTARTS ARE NORMAL",
            1,
            DIM);

        flip_draw_text(
            drm,
            centred_text_x(
                "KEEP CHARGER CONNECTED UNTIL COMPLETE",
                1),
            176,
            "KEEP CHARGER CONNECTED UNTIL COMPLETE",
            1,
            FG);
    }

    for (int i = 0;
         i < OPTIMIZER_ACTION_COUNT;
         ++i) {

        const char *label =
            optimizer_actions[i];

        if (i == 0 && have_result)
            label = "RUN OPTIMIZATION AGAIN";

        if (i == 1 && !have_result)
            label = "VIEW RESULTS (NONE)";

        int y = 282 + i * 36;

        if (i == selected) {
            flip_draw_rect(
                drm,
                105,
                y - 6,
                430,
                29,
                PANEL);
            flip_draw_rect_outline(
                drm,
                105,
                y - 6,
                430,
                29,
                1,
                FG);
        }

        flip_draw_text(
            drm,
            centred_text_x(label, 2),
            y,
            label,
            2,
            i == selected
                ? FG
                : DIM);
    }

    if (message && message[0]) {
        flip_draw_text(
            drm,
            centred_text_x(message, 1),
            430,
            message,
            1,
            FG);
    } else {
        flip_draw_text(
            drm,
            centred_text_x(
                "A SELECT   B BACK",
                1),
            452,
            "A SELECT   B BACK",
            1,
            DIM);
    }

    if (confirm_action != 0) {
        const char *confirm =
            confirm_action == 1
                ? "START CPU OPTIMIZATION?"
                : "RESET CPU OPTIMIZATION?";

        const char *detail =
            confirm_action == 1
                ? "KEEP CHARGER CONNECTED"
                : "STOCK VOLTAGES WILL BE USED";

        flip_draw_rect(
            drm,
            90,
            350,
            460,
            92,
            PANEL);
        flip_draw_rect_outline(
            drm,
            90,
            350,
            460,
            92,
            2,
            FG);

        flip_draw_text(
            drm,
            centred_text_x(confirm, 2),
            365,
            confirm,
            2,
            FG);
        flip_draw_text(
            drm,
            centred_text_x(detail, 1),
            394,
            detail,
            1,
            DIM);
        flip_draw_text(
            drm,
            centred_text_x(
                "UNPLUG CHARGER LATER TO CANCEL SAFELY",
                1),
            414,
            "UNPLUG CHARGER LATER TO CANCEL SAFELY",
            1,
            DIM);
        flip_draw_text(
            drm,
            centred_text_x(
                "A CONFIRM   B CANCEL",
                1),
            431,
            "A CONFIRM   B CANCEL",
            1,
            FG);
    }

    flip_osd_set_poweroff_progress(
        poweroff_progress);
    flip_osd_draw(drm);
    flip_drm_present(drm);
}

static void draw_home(
    flip_drm_t *drm,
    int selected,
    const char *message,
    double poweroff_progress)
{
    flip_draw_clear(drm, BG);

    flip_draw_text(
        drm,
        centred_text_x("FLIPOS", 5),
        28,
        "FLIPOS",
        5,
        FG);

    flip_draw_text(
        drm,
        centred_text_x("GAME LIBRARY", 2),
        76,
        "GAME LIBRARY",
        2,
        DIM);

    for (int i = 0;
         i < HOME_ITEM_COUNT;
         ++i) {

        int y =
            135 + i * 55;

        if (i == selected) {

            flip_draw_rect(
                drm,
                145,
                y - 8,
                350,
                42,
                PANEL);

            flip_draw_rect_outline(
                drm,
                145,
                y - 8,
                350,
                42,
                2,
                FG);
        }

        flip_draw_text(
            drm,
            centred_text_x(home_items[i], 3),
            y,
            home_items[i],
            3,
            i == selected ? FG : DIM);
    }

    if (message &&
        message[0]) {

        flip_draw_text(
            drm,
            190,
            365,
            message,
            1,
            FG);
    }

    flip_draw_text(
        drm,
        centred_text_x(
            "MENU + SELECT POWER OFF",
            1),
        420,
        "MENU + SELECT POWER OFF",
        1,
        DIM);

    flip_draw_text(
        drm,
        centred_text_x(
            "UP DOWN   A SELECT",
            1),
        452,
        "UP DOWN   A SELECT",
        1,
        DIM);

    flip_osd_set_poweroff_progress(
        poweroff_progress);

    /*
     * Global FlipOS OSD is composited last so hardware
     * feedback appears consistently over every frontend
     * screen.
     */
    flip_osd_draw(
        drm);

    flip_drm_present(drm);
}


static void draw_systems(
    flip_drm_t *drm,
    int selected,
    double poweroff_progress)
{
    flip_draw_clear(drm, BG);

    flip_draw_text(
        drm,
        centred_text_x("SYSTEMS", 4),
        28,
        "SYSTEMS",
        4,
        FG);

    char count_text[64];

    snprintf(
        count_text,
        sizeof(count_text),
        "%d SYSTEMS",
        available_system_count);

    flip_draw_text(
        drm,
        centred_text_x(count_text, 1),
        72,
        count_text,
        1,
        DIM);

    if (available_system_count <= 0) {

        flip_draw_text(
            drm,
            centred_text_x(
                "NO GAMES FOUND",
                2),
            190,
            "NO GAMES FOUND",
            2,
            FG);

        flip_draw_text(
            drm,
            centred_text_x(
                "ADD ROMS TO THE SD CARD",
                1),
            225,
            "ADD ROMS TO THE SD CARD",
            1,
            DIM);

    } else {

        for (int i = 0;
             i < available_system_count;
             ++i) {

            const flip_system_t *system =
                available_systems[i];

            if (!system)
                continue;

            int y =
                125 + i * 48;

            if (i == selected) {

                flip_draw_rect(
                    drm,
                    145,
                    y - 8,
                    350,
                    36,
                    PANEL);

                flip_draw_rect_outline(
                    drm,
                    145,
                    y - 8,
                    350,
                    36,
                    1,
                    FG);
            }

            const char *name =
                system->display_name
                    ? system->display_name
                    : system->id;

            flip_draw_text(
                drm,
                centred_text_x(name, 2),
                y,
                name,
                2,
                i == selected
                    ? FG
                    : DIM);
        }
    }

    flip_draw_text(
        drm,
        centred_text_x(
            "MENU + SELECT POWER OFF",
            1),
        420,
        "MENU + SELECT POWER OFF",
        1,
        DIM);

    flip_draw_text(
        drm,
        centred_text_x(
            "A SELECT   B BACK",
            1),
        452,
        "A SELECT   B BACK",
        1,
        DIM);

    flip_osd_set_poweroff_progress(
        poweroff_progress);

    flip_osd_draw(
        drm);

    flip_drm_present(drm);
}

static void draw_games(
    flip_drm_t *drm,
    const flip_system_t *system,
    int selected,
    double poweroff_progress)
{
    flip_draw_clear(drm, BG);

    const char *system_title =
        system &&
        system->display_name
            ? system->display_name
            : "GAMES";

    flip_draw_text(
        drm,
        centred_text_x(
            system_title,
            4),
        20,
        system_title,
        4,
        FG);

    char count_text[64];

    snprintf(
        count_text,
        sizeof(count_text),
        "%d GAMES",
        rom_count);

    flip_draw_text(
        drm,
        centred_text_x(count_text, 1),
        61,
        count_text,
        1,
        DIM);

    if (rom_count <= 0) {

        flip_draw_text(
            drm,
            175,
            190,
            "NO GB ROMS FOUND",
            2,
            FG);

        flip_draw_text(
            drm,
            175,
            225,
            "PUT ROMS IN ROMS GB",
            1,
            DIM);

    } else {

        int first =
            selected - ROWS_VISIBLE / 2;

        if (first < 0)
            first = 0;

        if (first >
            rom_count - ROWS_VISIBLE)
            first =
                rom_count - ROWS_VISIBLE;

        if (first < 0)
            first = 0;

        for (int row = 0;
             row < ROWS_VISIBLE;
             ++row) {

            int index =
                first + row;

            if (index >= rom_count)
                break;

            int y =
                92 + row * 34;

            if (index == selected) {

                flip_draw_rect(
                    drm,
                    55,
                    y - 5,
                    530,
                    29,
                    PANEL);

                flip_draw_rect_outline(
                    drm,
                    55,
                    y - 5,
                    530,
                    29,
                    1,
                    FG);
            }

            /*
             * Keep first browser version simple:
             * truncate long names to fit 640x480.
             */
            char visible[43];

            snprintf(
                visible,
                sizeof(visible),
                "%.40s",
                roms[index].display);

            flip_draw_text(
                drm,
                72,
                y,
                visible,
                2,
                index == selected
                    ? FG
                    : DIM);

        }
    }

    flip_draw_text(
        drm,
        centred_text_x(
            "MENU + SELECT POWER OFF",
            1),
        420,
        "MENU + SELECT POWER OFF",
        1,
        DIM);

    flip_draw_text(
        drm,
        centred_text_x(
            "A LAUNCH   B BACK",
            1),
        452,
        "A LAUNCH   B BACK",
        1,
        DIM);

    flip_osd_set_poweroff_progress(
        poweroff_progress);

    /*
     * Global FlipOS OSD is composited last so hardware
     * feedback appears consistently over every frontend
     * screen.
     */
    flip_osd_draw(
        drm);

    flip_drm_present(drm);
}

static void draw_recent(
    flip_drm_t *drm,
    const flip_system_t *system,
    int selected,
    double poweroff_progress)
{
    (void)system;

    flip_draw_clear(drm, BG);

    load_suspended_session();

    flip_draw_text(
        drm,
        centred_text_x("GAMESWITCHER", 3),
        20,
        "GAMESWITCHER",
        3,
        FG);

    char count_text[64];

    snprintf(
        count_text,
        sizeof(count_text),
        "%d GAMES",
        recent_count);

    flip_draw_text(
        drm,
        centred_text_x(
            count_text,
            1),
        61,
        count_text,
        1,
        DIM);

    if (recent_count <= 0) {

        flip_draw_text(
            drm,
            centred_text_x(
                "NO GAMES YET",
                2),
            190,
            "NO GAMES YET",
            2,
            FG);

        flip_draw_text(
            drm,
            centred_text_x(
                "LAUNCH A GAME FIRST",
                1),
            225,
            "LAUNCH A GAME FIRST",
            1,
            DIM);

    } else {

        int first =
            selected - ROWS_VISIBLE / 2;

        if (first < 0)
            first = 0;

        if (first >
            recent_count -
            ROWS_VISIBLE)
            first =
                recent_count -
                ROWS_VISIBLE;

        if (first < 0)
            first = 0;

        for (int row = 0;
             row < ROWS_VISIBLE;
             ++row) {

            int index =
                first + row;

            if (index >=
                recent_count)
                break;

            int y =
                92 + row * 34;

            if (index ==
                selected) {

                flip_draw_rect(
                    drm,
                    55,
                    y - 5,
                    530,
                    29,
                    PANEL);

                flip_draw_rect_outline(
                    drm,
                    55,
                    y - 5,
                    530,
                    29,
                    1,
                    FG);
            }

            char visible[43];

            snprintf(
                visible,
                sizeof(visible),
                "%.40s",
                recent_roms[index].display);

            flip_draw_text(
                drm,
                72,
                y,
                visible,
                2,
                index == selected
                    ? FG
                    : DIM);

            if (flip_system_resume_available(
                    recent_roms[index].system,
                    recent_roms[index].path)) {

                char session_state[32];

                int have_session =
                    recent_session_registry_state(
                        &recent_roms[index],
                        session_state,
                        sizeof(session_state)) > 0;

                int is_suspended =
                    have_session &&
                    strcmp(
                        session_state,
                        "suspended") == 0;

                int needs_recovery =
                    have_session &&
                    (strcmp(
                         session_state,
                         "interrupted") == 0 ||
                     strcmp(
                         session_state,
                         "signaled") == 0 ||
                     strcmp(
                         session_state,
                         "error") == 0);

                const char *status =
                    is_suspended
                        ? "SUSPENDED"
                        : needs_recovery
                            ? "RECOVER"
                            : "RESUME";

                int status_x =
                    is_suspended
                        ? 470
                        : needs_recovery
                            ? 488
                            : 500;

                flip_draw_text(
                    drm,
                    status_x,
                    y + 4,
                    status,
                    1,
                    index == selected
                        ? FG
                        : DIM);
            }
        }
    }

    flip_draw_text(
        drm,
        centred_text_x(
            "MENU + SELECT POWER OFF",
            1),
        420,
        "MENU + SELECT POWER OFF",
        1,
        DIM);

    const char *recent_help =
        "B BACK";

    if (recent_count > 0) {

        int can_resume =
            flip_system_resume_available(
                recent_roms[selected].system,
                recent_roms[selected].path);

        int is_recoverable =
            recent_session_is_recoverable(
                &recent_roms[selected]);

        if (is_recoverable) {

            recent_help =
                "A RESUME   X CLOSE   B BACK";

        } else if (
            can_resume) {

            recent_help =
                "A RESUME   X REMOVE   B BACK";

        } else {

            recent_help =
                "A LAUNCH   X REMOVE   B BACK";
        }
    }

    flip_draw_text(
        drm,
        centred_text_x(
            recent_help,
            1),
        452,
        recent_help,
        1,
        DIM);

    if (gameswitcher_confirm !=
        GAMESWITCHER_CONFIRM_NONE) {

        const char *confirm_text =
            gameswitcher_confirm ==
                GAMESWITCHER_CONFIRM_CLOSE
                ? "CLOSE SESSION?"
                : "REMOVE GAME?";

        flip_draw_rect(
            drm,
            115,
            365,
            410,
            76,
            PANEL);

        flip_draw_rect_outline(
            drm,
            115,
            365,
            410,
            76,
            1,
            FG);

        flip_draw_text(
            drm,
            centred_text_x(
                confirm_text,
                2),
            382,
            confirm_text,
            2,
            FG);

        flip_draw_text(
            drm,
            centred_text_x(
                "A CONFIRM   B CANCEL",
                1),
            418,
            "A CONFIRM   B CANCEL",
            1,
            DIM);
    }

    flip_osd_set_poweroff_progress(
        poweroff_progress);

    /*
     * Global FlipOS OSD is composited last so hardware
     * feedback appears consistently over every frontend
     * screen.
     */
    flip_osd_draw(
        drm);

    flip_drm_present(drm);
}

static int launch_game_once(
    flip_drm_t *drm,
    const rom_entry_t *rom,
    int automatic_resume)
{
    if (!rom ||
        !rom->system ||
        !rom->system->runner) {

        fprintf(
            stderr,
            "FRONTEND_GAME system lookup failed\n");

        return -1;
    }

    const flip_system_t *system =
        rom->system;

    const char *runner =
        system->runner;

    char *const argv[] = {
        (char *)runner,
        (char *)rom->path,
        NULL
    };

    flip_session_metadata_t metadata = {
        .system = system,
        .rom_path = rom->path,
        .automatic_resume = automatic_resume
    };

    flip_session_result_t result;

    fprintf(
        stderr,
        "FRONTEND_GAME launch=%s rom=%s\n",
        runner,
        rom->path);

    int runtime_profile_applied =
        optimizer_run_tool(
            OPTIMIZER_RUNTIME_TOOL,
            "apply",
            NULL) == 0;

    fprintf(
        stderr,
        "FRONTEND_RUNTIME_PROFILE applied=%d\n",
        runtime_profile_applied);

    if (flip_session_launch(
            drm,
            runner,
            argv,
            &metadata,
            &result) < 0) {

        fprintf(
            stderr,
            "FRONTEND_GAME session failed\n");

        if (runtime_profile_applied)
            (void)optimizer_run_tool(
                OPTIMIZER_RUNTIME_TOOL,
                "restore",
                NULL);

        return -1;
    }

    if (runtime_profile_applied &&
        optimizer_run_tool(
            OPTIMIZER_RUNTIME_TOOL,
            "restore",
            NULL) < 0) {
        fprintf(
            stderr,
            "FRONTEND_RUNTIME_PROFILE restore_failed=1\n");
    } else if (runtime_profile_applied) {
        fprintf(
            stderr,
            "FRONTEND_RUNTIME_PROFILE restored=1\n");
    }

    if (result.signaled) {

        fprintf(
            stderr,
            "FRONTEND_GAME signaled signal=%d\n",
            result.signal_number);

    } else {

        fprintf(
            stderr,
            "FRONTEND_GAME exited code=%d\n",
            result.exit_code);
    }

    fprintf(
        stderr,
        "FRONTEND_GAME session_action=%d\n",
        result.action);

    return (int)result.action;
}

static int launch_game(
    flip_drm_t *drm,
    const rom_entry_t *rom)
{
    int automatic_resume = 0;

    for (;;) {

        int result =
            launch_game_once(
                drm,
                rom,
                automatic_resume);

        /* The intent applies to exactly one post-wake relaunch. */
        automatic_resume = 0;

        if (result != FLIP_SESSION_ACTION_SUSPEND)
            return result;

        /*
         * Refresh persistent hall state after the session
         * returned with a suspend/GameSwitcher action.
         */
        flip_input_state_t lid_state;

        memset(
            &lid_state,
            0,
            sizeof(lid_state));

        flip_power_lid_state_t lid =
            flip_power_refresh_lid_state(
                &lid_state);

        if (lid == FLIP_POWER_LID_ERROR) {

            fprintf(
                stderr,
                "FRONTEND_STANDBY lid poll failed\n");

            return result;
        }

        /*
         * A suspend action without a confirmed closed lid
         * means MENU requested the normal GameSwitcher.
         */
        if (lid != FLIP_POWER_LID_CLOSED) {

            fprintf(
                stderr,
                "FRONTEND_GAMESWITCHER requested\n");

            return result;
        }

        fprintf(
            stderr,
            "FRONTEND_STANDBY lid closed after suspend\n");

        if (flip_power_sleep_until_lid_open(
                &lid_state) < 0) {

            fprintf(
                stderr,
                "FRONTEND_STANDBY wait failed\n");

            return -1;
        }

        fprintf(
            stderr,
            "FRONTEND_STANDBY auto resume rom=%s\n",
            rom->path);

        /*
         * This next launch is a continuation of the lid-suspended session,
         * not a new user launch decision. Skip Game Options exactly once.
         */
        automatic_resume = 1;

        /*
         * Loop back through the normal session launcher.
         * Gambatte will restore the saved per-ROM state.
         */
    }
}

int main(void)
{
    signal(
        SIGINT,
        stop_handler);

    signal(
        SIGTERM,
        stop_handler);

    flip_drm_t drm;

    if (flip_drm_open(&drm) < 0) {

        fprintf(
            stderr,
            "FRONTEND: DRM initialization failed\n");

        return 1;
    }

    flip_input_state_t state;
    memset(
        &state,
        0,
        sizeof(state));

    if (flip_input_open() < 0) {

        fprintf(
            stderr,
            "FRONTEND: input initialization failed\n");

        flip_drm_close(&drm);

        return 2;
    }

    screen_t screen =
        SCREEN_HOME;

    int home_selected = 0;
    int game_selected = 0;
    int recent_selected = 0;
    int settings_selected = 0;
    int optimizer_selected = 0;
    int optimizer_confirm = 0;

    int prev_up = 0;
    int prev_down = 0;
    int prev_a = 0;
    int prev_b = 0;
    int prev_x = 0;

    flip_hotkeys_t global_hotkeys;

    flip_hotkeys_init(
        &global_hotkeys);

    /*
     * Tracks the previous OSD visibility state so the
     * frontend performs one clean redraw when the OSD
     * expires.
     */
    int osd_was_active = 0;

    int prev_lid_ready = 0;
    int prev_lid_closed = 0;

    int poweroff_active = 0;
    double poweroff_started = 0.0;

    char message[64] = "";
    char optimizer_message[96] = "";

    const flip_system_t *game_system =
        flip_system_default();

    if (!game_system) {

        fprintf(
            stderr,
            "FRONTEND: default system registry lookup failed\n");

        flip_input_close();
        flip_drm_close(&drm);

        return 3;
    }

    flip_osd_init();

    if (flip_settings_init() == 0) {

        flip_power_set_brightness(
            flip_settings_get_brightness());
    }

    if (flip_session_recover_current() < 0) {

        fprintf(
            stderr,
            "FRONTEND: session recovery failed\n");
    }

    int registry_reconcile =
        flip_session_reconcile_registry();

    if (registry_reconcile < 0) {

        fprintf(
            stderr,
            "FRONTEND: session registry reconcile failed\n");

    } else if (registry_reconcile > 0) {

        fprintf(
            stderr,
            "FRONTEND: session registry reconciled changes=%d\n",
            registry_reconcile);
    }

    /*
     * HOME does not need a ROM listing. Defer library
     * validation/loading until the user enters Games.
     */
    rom_count = 0;

    load_recent_history();
    load_suspended_session();

    if (access(
            OPTIMIZER_UI_RETURN_PATH,
            F_OK) == 0) {
        screen = SCREEN_OPTIMIZER;
        (void)unlink(
            OPTIMIZER_UI_RETURN_PATH);
        snprintf(
            optimizer_message,
            sizeof(optimizer_message),
            "OPTIMIZATION COMPLETE");
        draw_optimizer(
            &drm,
            optimizer_selected,
            optimizer_confirm,
            optimizer_message,
            0.0);
    } else {
        draw_home(
            &drm,
            home_selected,
            message,
            0.0);
    }

    while (running) {

        int changed =
            flip_input_poll(
                &state,
                30);

        if (changed < 0) {

            fprintf(
                stderr,
                "FRONTEND: input poll failed\n");

            break;
        }

        /*
         * Global frontend lid handling.
         *
         * HOME, GAMES and RECENT enter the same proven
         * deep-suspend path used after emulator suspension.
         */
        if (state.lid_ready) {

            if (prev_lid_ready &&
                !prev_lid_closed &&
                state.lid_closed) {

                fprintf(
                    stderr,
                    "FRONTEND_LID CLOSED\n");

                if (flip_power_sleep_until_lid_open(
                        &state) < 0) {

                    fprintf(
                        stderr,
                        "FRONTEND_LID standby failed\n");

                    break;
                }

                fprintf(
                    stderr,
                    "FRONTEND_LID OPEN\n");

                /*
                 * Clear stale button state accumulated across
                 * kernel suspend.
                 */
                memset(
                    &state,
                    0,
                    sizeof(state));

                flip_hotkeys_init(
                    &global_hotkeys);

                prev_up = 0;
                prev_down = 0;
                prev_a = 0;
                prev_b = 0;

                poweroff_active = 0;

                /*
                 * Re-prime authoritative lid state after wake.
                 */
                flip_input_poll(
                    &state,
                    0);

                prev_lid_ready =
                    state.lid_ready;

                prev_lid_closed =
                    state.lid_closed;

                /*
                 * Restore exactly the frontend screen and
                 * selection that was active before sleep.
                 */
                if (screen ==
                    SCREEN_HOME) {

                    draw_home(
                        &drm,
                        home_selected,
                        message,
                        0.0);

                } else if (
                    screen ==
                        SCREEN_SYSTEMS) {

                    refresh_available_systems();

                    draw_systems(
                        &drm,
                        system_selected,
                        0.0);

                } else if (
                    screen ==
                        SCREEN_GAMES) {

                    draw_games(
                        &drm,
                        game_system,
                        game_selected,
                        0.0);

                } else if (
                    screen ==
                        SCREEN_RECENT) {

                    draw_recent(
                        &drm,
                        game_system,
                        recent_selected,
                        0.0);

                } else if (
                    screen ==
                        SCREEN_SETTINGS) {

                    draw_settings(
                        &drm,
                        settings_selected,
                        0.0);

                } else {

                    draw_optimizer(
                        &drm,
                        optimizer_selected,
                        optimizer_confirm,
                        optimizer_message,
                        0.0);
                }

                continue;
            }

            prev_lid_ready = 1;

            prev_lid_closed =
                state.lid_closed;
        }

        int up =
            state.down[
                FLIP_BTN_UP];

        int down =
            state.down[
                FLIP_BTN_DOWN];

        int a =
            state.down[
                FLIP_BTN_A];

        int b =
            state.down[
                FLIP_BTN_B];

        int x =
            state.down[
                FLIP_BTN_X];

        /*
         * FlipOS-global hardware shortcuts.
         *
         *   VOL+        -> volume up
         *   VOL-        -> volume down
         *   MENU+VOL+   -> brightness up
         *   MENU+VOL-   -> brightness down
         *
         * The frontend does not own the policy; it only
         * feeds input into the shared FlipOS hotkey layer.
         */
        flip_hotkeys_result_t hotkey_result;

        if (flip_hotkeys_handle(
                &global_hotkeys,
                &state,
                &hotkey_result) < 0) {

            fprintf(
                stderr,
                "FRONTEND: global hotkey handling failed\n");
        }

        int redraw = 0;

        /*
         * Static frontend screens need active redraws while
         * the OSD is visible, plus one final redraw when its
         * timeout expires so the old bar is erased.
         */
        int osd_active =
            flip_osd_active();

        if (osd_active ||
            osd_was_active) {

            redraw = 1;
        }

        osd_was_active =
            osd_active;

        if (up &&
            !prev_up) {

            if (screen == SCREEN_HOME) {

                home_selected--;

                if (home_selected < 0)
                    home_selected =
                        HOME_ITEM_COUNT - 1;

            } else if (
                screen ==
                    SCREEN_SYSTEMS &&
                available_system_count > 0) {

                system_selected--;

                if (system_selected < 0)
                    system_selected =
                        available_system_count - 1;

            } else if (
                screen ==
                    SCREEN_GAMES &&
                rom_count > 0) {

                game_selected--;

                if (game_selected < 0)
                    game_selected =
                        rom_count - 1;

            } else if (
                screen ==
                    SCREEN_RECENT &&
                recent_count > 0) {

                recent_selected--;

                if (recent_selected < 0)
                    recent_selected =
                        recent_count - 1;

            } else if (
                screen ==
                    SCREEN_SETTINGS) {

                settings_selected--;

                if (settings_selected < 0)
                    settings_selected =
                        SETTINGS_ITEM_COUNT - 1;

            } else if (
                screen ==
                    SCREEN_OPTIMIZER &&
                optimizer_confirm == 0) {

                optimizer_selected--;

                if (optimizer_selected < 0)
                    optimizer_selected =
                        OPTIMIZER_ACTION_COUNT - 1;
            }

            redraw = 1;
        }

        if (down &&
            !prev_down) {

            if (screen == SCREEN_HOME) {

                home_selected++;

                if (home_selected >=
                    HOME_ITEM_COUNT)
                    home_selected = 0;

            } else if (
                screen ==
                    SCREEN_SYSTEMS &&
                available_system_count > 0) {

                system_selected++;

                if (system_selected >=
                    available_system_count) {

                    system_selected = 0;
                }

            } else if (
                screen ==
                    SCREEN_GAMES &&
                rom_count > 0) {

                game_selected++;

                if (game_selected >=
                    rom_count)
                    game_selected = 0;

            } else if (
                screen ==
                    SCREEN_RECENT &&
                recent_count > 0) {

                recent_selected++;

                if (recent_selected >=
                    recent_count)
                    recent_selected = 0;

            } else if (
                screen ==
                    SCREEN_SETTINGS) {

                settings_selected++;

                if (settings_selected >=
                    SETTINGS_ITEM_COUNT)
                    settings_selected = 0;

            } else if (
                screen ==
                    SCREEN_OPTIMIZER &&
                optimizer_confirm == 0) {

                optimizer_selected++;

                if (optimizer_selected >=
                    OPTIMIZER_ACTION_COUNT)
                    optimizer_selected = 0;
            }

            redraw = 1;
        }

        if (b &&
            !prev_b &&
            screen == SCREEN_OPTIMIZER &&
            optimizer_confirm != 0) {

            optimizer_confirm = 0;
            optimizer_message[0] = '\0';
            redraw = 1;
        }

        if (b &&
            !prev_b &&
            gameswitcher_confirm ==
                GAMESWITCHER_CONFIRM_NONE &&
            optimizer_confirm == 0) {

            if (screen ==
                SCREEN_GAMES) {

                refresh_available_systems();

                screen =
                    SCREEN_SYSTEMS;

                redraw = 1;

            } else if (
                screen ==
                    SCREEN_SYSTEMS ||
                screen ==
                    SCREEN_RECENT) {

                screen =
                    SCREEN_HOME;

                message[0] =
                    '\0';

                redraw = 1;

            } else if (
                screen ==
                    SCREEN_SETTINGS) {

                screen =
                    SCREEN_HOME;

                message[0] =
                    '\0';

                redraw = 1;

            } else if (
                screen ==
                    SCREEN_OPTIMIZER) {

                screen =
                    SCREEN_SETTINGS;

                optimizer_message[0] =
                    '\0';

                redraw = 1;
            }
        }

        int confirm_handled = 0;

        if (screen == SCREEN_OPTIMIZER &&
            optimizer_confirm != 0 &&
            a &&
            !prev_a) {

            int action =
                optimizer_confirm;

            optimizer_confirm = 0;
            confirm_handled = 1;

            if (action == 1) {
                int start_rc =
                    optimizer_start_fresh();

                if (start_rc == 1) {
                    snprintf(
                        optimizer_message,
                        sizeof(optimizer_message),
                        "CONNECT CHARGER - BATTERY 30%%+");
                    redraw = 1;

                } else if (start_rc < 0) {
                    snprintf(
                        optimizer_message,
                        sizeof(optimizer_message),
                        "OPTIMIZER START FAILED");
                    redraw = 1;

                } else {
                    snprintf(
                        optimizer_message,
                        sizeof(optimizer_message),
                        "STARTING OPTIMIZATION...");
                    draw_optimizer(
                        &drm,
                        optimizer_selected,
                        0,
                        optimizer_message,
                        0.0);

                    fprintf(
                        stderr,
                        "FRONTEND_OPTIMIZER start requested charger=1\n");
                    fflush(stderr);
                    sync();
                    reboot(RB_AUTOBOOT);
                }

            } else if (action == 2) {
                if (optimizer_reset_all() < 0) {
                    snprintf(
                        optimizer_message,
                        sizeof(optimizer_message),
                        "RESET FAILED");
                } else {
                    snprintf(
                        optimizer_message,
                        sizeof(optimizer_message),
                        "RESET TO STOCK");
                }

                redraw = 1;
            }
        }

        /*
         * B cancels a GameSwitcher confirmation instead
         * of leaving the screen.
         */
        if (gameswitcher_confirm !=
                GAMESWITCHER_CONFIRM_NONE &&
            b &&
            !prev_b) {

            gameswitcher_confirm =
                GAMESWITCHER_CONFIRM_NONE;

            gameswitcher_confirm_system[0] =
                '\0';

            gameswitcher_confirm_rom[0] =
                '\0';

            fprintf(
                stderr,
                "FRONTEND_GAMESWITCHER action cancelled\n");

            confirm_handled = 1;
            redraw = 1;
        }

        /*
         * X operates on every GameSwitcher entry.
         *
         * Suspended entry -> CLOSE SESSION
         * Other entry     -> REMOVE FROM GAMESWITCHER
         */
        if (gameswitcher_confirm ==
                GAMESWITCHER_CONFIRM_NONE &&
            x &&
            !prev_x &&
            screen == SCREEN_RECENT &&
            recent_count > 0) {

            const rom_entry_t *target =
                &recent_roms[
                    recent_selected];

            copy_string(
                gameswitcher_confirm_system,
                sizeof(gameswitcher_confirm_system),
                target->system &&
                target->system->id
                    ? target->system->id
                    : "");

            copy_string(
                gameswitcher_confirm_rom,
                sizeof(gameswitcher_confirm_rom),
                target->path);

            int is_recoverable =
                recent_session_is_recoverable(
                    target);

            gameswitcher_confirm =
                is_recoverable
                    ? GAMESWITCHER_CONFIRM_CLOSE
                    : GAMESWITCHER_CONFIRM_REMOVE;

            fprintf(
                stderr,
                "FRONTEND_GAMESWITCHER action requested=%s rom=%s\n",
                is_recoverable
                    ? "close"
                    : "remove",
                gameswitcher_confirm_rom);

            redraw = 1;
        }

        /*
         * A confirms the requested GameSwitcher action.
         *
         * Neither CLOSE nor REMOVE deletes emulator
         * savestate/SRAM data.
         */
        if (gameswitcher_confirm !=
                GAMESWITCHER_CONFIRM_NONE &&
            a &&
            !prev_a) {

            int action_ok = 0;

            if (gameswitcher_confirm ==
                GAMESWITCHER_CONFIRM_CLOSE) {

                const flip_system_t *target_system =
                    flip_system_find(
                        gameswitcher_confirm_system);

                int close_result =
                    target_system
                        ? flip_session_close_registered(
                            target_system,
                            gameswitcher_confirm_rom)
                        : -1;

                if (close_result > 0) {

                    fprintf(
                        stderr,
                        "FRONTEND_GAMESWITCHER closed=%s\n",
                        gameswitcher_confirm_rom);

                    action_ok = 1;

                } else if (
                    close_result < 0) {

                    fprintf(
                        stderr,
                        "FRONTEND_GAMESWITCHER close failed\n");
                }

            } else {

                action_ok = 1;

                fprintf(
                    stderr,
                    "FRONTEND_GAMESWITCHER removing=%s\n",
                    gameswitcher_confirm_rom);
            }

            if (action_ok) {

                const flip_system_t *target_system =
                    flip_system_find(
                        gameswitcher_confirm_system);

                if (!target_system ||
                    flip_session_remove_recent(
                        target_system,
                        gameswitcher_confirm_rom) < 0) {

                    fprintf(
                        stderr,
                        "FRONTEND_GAMESWITCHER history remove failed\n");

                } else {

                    fprintf(
                        stderr,
                        "FRONTEND_GAMESWITCHER removed=%s\n",
                        gameswitcher_confirm_rom);
                }

                gameswitcher_confirm =
                    GAMESWITCHER_CONFIRM_NONE;

                gameswitcher_confirm_system[0] =
                    '\0';

                gameswitcher_confirm_rom[0] =
                    '\0';

                load_suspended_session();
                load_recent_history();

                if (recent_selected >=
                    recent_count) {

                    recent_selected =
                        recent_count > 0
                            ? recent_count - 1
                            : 0;
                }

                redraw = 1;
            }

            confirm_handled = 1;
        }

        if (a &&
            !prev_a &&
            !confirm_handled &&
            gameswitcher_confirm ==
                GAMESWITCHER_CONFIRM_NONE) {

            if (screen ==
                SCREEN_SYSTEMS &&
                available_system_count > 0) {

                game_system =
                    available_systems[
                        system_selected];

                load_system_roms(
                    &drm,
                    game_system);

                game_selected = 0;

                screen =
                    SCREEN_GAMES;

                redraw = 1;

            } else if (screen ==
                SCREEN_HOME) {

                if (home_selected == 0) {

                    refresh_available_systems();

                    if (system_selected >=
                        available_system_count) {

                        system_selected = 0;
                    }

                    screen =
                        SCREEN_SYSTEMS;

                    redraw = 1;

                } else if (
                    home_selected == 1) {

                    load_recent_history();

                    screen =
                        SCREEN_RECENT;

                    redraw = 1;

                } else if (
                    home_selected == 2) {

                    screen =
                        SCREEN_SETTINGS;

                    settings_selected = 0;
                    message[0] = '\0';
                    fprintf(
                        stderr,
                        "FRONTEND_SETTINGS entered\n");
                    redraw = 1;

                } else {

                    snprintf(
                        message,
                        sizeof(message),
                        "COMING SOON");

                    redraw = 1;
                }

            } else if (
                screen ==
                    SCREEN_SETTINGS) {

                if (settings_selected == 0) {
                    screen =
                        SCREEN_OPTIMIZER;
                    optimizer_selected = 0;
                    optimizer_confirm = 0;
                    optimizer_message[0] = '\0';
                    fprintf(
                        stderr,
                        "FRONTEND_OPTIMIZER entered\n");
                } else {
                    screen =
                        SCREEN_HOME;
                }

                redraw = 1;

            } else if (
                screen ==
                    SCREEN_OPTIMIZER &&
                optimizer_confirm == 0) {

                if (optimizer_selected == 0) {
                    optimizer_confirm = 1;
                    optimizer_message[0] = '\0';

                } else if (
                    optimizer_selected == 1) {

                    flip_optimizer_result_t result;

                    if (optimizer_result_load(
                            &result)) {
                        snprintf(
                            optimizer_message,
                            sizeof(optimizer_message),
                            "RESULTS SHOWN ABOVE");
                    } else {
                        snprintf(
                            optimizer_message,
                            sizeof(optimizer_message),
                            "NO RESULTS AVAILABLE");
                    }

                } else if (
                    optimizer_selected == 2) {

                    flip_optimizer_result_t result;

                    if (optimizer_result_load(
                            &result)) {
                        optimizer_confirm = 2;
                        optimizer_message[0] = '\0';
                    } else {
                        snprintf(
                            optimizer_message,
                            sizeof(optimizer_message),
                            "NOTHING TO RESET");
                    }

                } else {
                    screen =
                        SCREEN_SETTINGS;
                    optimizer_message[0] = '\0';
                }

                redraw = 1;

            } else if (
                screen ==
                    SCREEN_RECENT &&
                recent_count > 0) {

                flip_draw_clear(
                    &drm,
                    BG);

                const char *launch_text =
                    flip_system_resume_available(
                        recent_roms[
                            recent_selected
                        ].system,
                        recent_roms[
                            recent_selected
                        ].path)
                        ? "RESUMING"
                        : "LAUNCHING";

                flip_draw_text(
                    &drm,
                    centred_text_x(
                        launch_text,
                        3),
                    185,
                    launch_text,
                    3,
                    FG);

                flip_draw_text(
                    &drm,
                    centred_text_x(
                        recent_roms[recent_selected].display,
                        1),
                    235,
                    recent_roms[recent_selected].display,
                    1,
                    DIM);

                flip_drm_present(
                    &drm);

                if (launch_game(
                        &drm,
                        &recent_roms[recent_selected]) < 0) {

                    fprintf(
                        stderr,
                        "FRONTEND_RECENT lifecycle failed\n");

                    return 3;
                }

                memset(
                    &state,
                    0,
                    sizeof(state));

                flip_hotkeys_init(
                    &global_hotkeys);

                prev_up = 0;
                prev_down = 0;
                prev_a = 0;
                prev_b = 0;

                poweroff_active = 0;

                load_recent_history();

                if (recent_selected >=
                    recent_count)
                    recent_selected =
                        recent_count > 0
                            ? recent_count - 1
                            : 0;

                draw_recent(
                    &drm,
                    game_system,
                    recent_selected,
                    0.0);

                continue;

            } else if (
                screen ==
                    SCREEN_GAMES &&
                rom_count > 0) {

                /*
                 * Draw an immediate launch indication before
                 * releasing DRM.
                 */
                flip_draw_clear(
                    &drm,
                    BG);

                flip_draw_text(
                    &drm,
                    195,
                    185,
                    "LAUNCHING",
                    3,
                    FG);

                flip_draw_text(
                    &drm,
                    110,
                    235,
                    roms[
                        game_selected
                    ].display,
                    1,
                    DIM);

                flip_drm_present(
                    &drm);

                int game_result =
                    launch_game(
                        &drm,
                        &roms[
                            game_selected
                        ]);

                if (game_result < 0) {

                    fprintf(
                        stderr,
                        "FRONTEND_GAME lifecycle failed\n");

                    return 3;
                }

                /*
                 * Hardware was reopened, so clear all stale
                 * button/edge state before accepting input.
                 */
                memset(
                    &state,
                    0,
                    sizeof(state));

                flip_hotkeys_init(
                    &global_hotkeys);

                prev_up = 0;
                prev_down = 0;
                prev_a = 0;
                prev_b = 0;

                poweroff_active = 0;

                /*
                 * Rescan in case SD library changed.
                 */
                load_system_roms(
                    &drm,
                    game_system);

                if (game_result == FLIP_SESSION_ACTION_SUSPEND) {

                    load_recent_history();

                    recent_selected = 0;

                    select_suspended_recent(
                        &recent_selected);

                    screen =
                        SCREEN_RECENT;

                    fprintf(
                        stderr,
                        "FRONTEND_GAMESWITCHER entered\n");

                    draw_recent(
                        &drm,
                        game_system,
                        recent_selected,
                        0.0);

                    continue;
                }

                if (game_selected >=
                    rom_count)
                    game_selected =
                        rom_count > 0
                            ? rom_count - 1
                            : 0;

                draw_games(
                    &drm,
                    game_system,
                    game_selected,
                    0.0);

                continue;
            }
        }

        prev_up = up;
        prev_down = down;
        prev_a = a;
        prev_b = b;
        prev_x = x;

        /*
         * Existing FlipOS clean shutdown gesture.
         */
        if (state.down[
                FLIP_BTN_MENU] &&
            state.down[
                FLIP_BTN_SELECT]) {

            double now =
                monotonic_seconds();

            if (!poweroff_active) {

                poweroff_active = 1;

                poweroff_started =
                    now;

                fprintf(
                    stderr,
                    "FRONTEND: poweroff hold started\n");
            }

            double held =
                now -
                poweroff_started;

            double progress =
                held /
                POWEROFF_HOLD_SECONDS;

            if (screen ==
                SCREEN_HOME) {

                draw_home(
                    &drm,
                    home_selected,
                    message,
                    progress);

            } else if (
                screen ==
                    SCREEN_SYSTEMS) {

                draw_systems(
                    &drm,
                    system_selected,
                    progress);

            } else if (
                screen ==
                    SCREEN_GAMES) {

                draw_games(
                    &drm,
                    game_system,
                    game_selected,
                    progress);

            } else if (
                screen ==
                    SCREEN_RECENT) {

                draw_recent(
                    &drm,
                    game_system,
                    recent_selected,
                    progress);

            } else if (
                screen ==
                    SCREEN_SETTINGS) {

                draw_settings(
                    &drm,
                    settings_selected,
                    progress);

            } else {

                draw_optimizer(
                    &drm,
                    optimizer_selected,
                    optimizer_confirm,
                    optimizer_message,
                    progress);
            }

            if (held >=
                POWEROFF_HOLD_SECONDS) {

                power_off_now();
                break;
            }

        } else {

            if (poweroff_active) {

                fprintf(
                    stderr,
                    "FRONTEND: poweroff hold cancelled\n");

                redraw = 1;
            }

            poweroff_active = 0;
        }

        if (redraw &&
            !poweroff_active) {

            if (screen ==
                SCREEN_HOME) {

                draw_home(
                    &drm,
                    home_selected,
                    message,
                    0.0);

            } else if (
                screen ==
                    SCREEN_SYSTEMS) {

                draw_systems(
                    &drm,
                    system_selected,
                    0.0);

            } else if (
                screen ==
                    SCREEN_GAMES) {

                draw_games(
                    &drm,
                    game_system,
                    game_selected,
                    0.0);

            } else if (
                screen ==
                    SCREEN_RECENT) {

                draw_recent(
                    &drm,
                    game_system,
                    recent_selected,
                    0.0);

            } else if (
                screen ==
                    SCREEN_SETTINGS) {

                draw_settings(
                    &drm,
                    settings_selected,
                    0.0);

            } else {

                draw_optimizer(
                    &drm,
                    optimizer_selected,
                    optimizer_confirm,
                    optimizer_message,
                    0.0);
            }
        }
    }

    flip_input_close();
    flip_drm_close(&drm);

    return 0;
}
