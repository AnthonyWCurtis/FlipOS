#define _GNU_SOURCE

#include "flip_drm.h"
#include "flip_draw.h"
#include "flip_input.h"
#include "flip_hotkeys.h"
#include "flip_keyboard.h"
#include "flip_setup.h"
#include "flip_time.h"
#include "flip_osd.h"
#include "flip_session.h"
#include "flip_game_stats.h"
#include "flip_paths.h"
#include "flip_session_registry.h"
#include "flip_settings.h"
#include "flip_system.h"
#include "flip_library.h"
#include "flip_log.h"
#include "flip_power.h"
#include "flip_power_policy.h"
#include "flip_battery.h"
#include "flip_optimizer.h"
#include "flip_wifi.h"
#include "flip_bios.h"

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
#define ROWS_VISIBLE 7

/*
 * Shared 640x480 typography scale.
 * Keep onboarding, menus and tools visually consistent at handheld distance.
 */
#define UI_SCALE_HERO      5
#define UI_SCALE_TITLE     4
#define UI_SCALE_SECTION   3
#define UI_SCALE_BODY      2
#define UI_SCALE_META      2
#define UI_SCALE_HINT      2
#define UI_FOOTER_Y      448

#define POWEROFF_HOLD_SECONDS 1.5
#define FLIP_DEV_SUPPRESS_LOW_BATTERY_POWEROFF "/tmp/flipos-dev-suppress-low-battery-poweroff"
#define FLIP_DEV_SCREEN_STATE "/tmp/flipos-dev-screen"
#define FLIP_DEV_FORCE_FIRST_BOOT "/tmp/flipos-dev-force-first-boot"
#define FLIP_UPDATE_PACKAGE "/mnt/SDCARD/FlipOSUpdate"
#define FLIP_UPDATE_MANIFEST FLIP_UPDATE_PACKAGE "/manifest"
#define FLIP_UPDATE_INSTALLER FLIP_BIN_ROOT "/flip-update-install"
#define FLIP_UPDATE_FETCHER FLIP_BIN_ROOT "/flip-update-fetch"
#define FLIP_VERSION_FILE FLIP_STORAGE_ROOT "/VERSION"
#define FLIP_UPDATE_AVAILABLE FLIP_STATE_DATA_ROOT "/update/available"

typedef enum {
    SCREEN_HOME = 0,
    SCREEN_SYSTEMS,
    SCREEN_GAMES,
    SCREEN_RECENT,
    SCREEN_STATS,
    SCREEN_GAME_DETAILS,
    SCREEN_SETTINGS,
    SCREEN_SETTINGS_CONNECTIVITY,
    SCREEN_SETTINGS_SYSTEM,
    SCREEN_SETTINGS_TIME,
    SCREEN_SETTINGS_BIOS,
    SCREEN_SETTINGS_INTERFACE,
    SCREEN_WIFI,
    SCREEN_WIFI_PASSWORD,
    SCREEN_FIRST_BOOT_WELCOME,
    SCREEN_FIRST_BOOT_WIFI,
    SCREEN_FIRST_BOOT_TIME,
    SCREEN_FIRST_BOOT_PREFS,
    SCREEN_FIRST_BOOT_BIOS,
    SCREEN_FIRST_BOOT_READY,
    SCREEN_UPDATE,
    SCREEN_OPTIMIZER,
    SCREEN_OPTIMIZER_RESULTS
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

static flip_wifi_network_t wifi_networks[FLIP_WIFI_MAX_NETWORKS];
static int wifi_network_count = 0;
static int wifi_selected = 0;
static flip_wifi_status_t wifi_status;
static int wifi_topbar_signal_level = -1;
static screen_t wifi_return_screen = SCREEN_SETTINGS;
static char wifi_message[96] = "";
static char wifi_password[64] = "";
static char wifi_password_message[96] = "";
static flip_keyboard_t wifi_keyboard;
static char update_message[128] = "";
static int update_complete = 0;
static int update_confirm = 0;

static flip_time_status_t time_status;
static int time_selected = 0;
static int time_timezone_index = 0;
static int time_manual_editing = 0;
static int time_manual_field = 0;
static struct tm time_manual_value;
static char time_message[128] = "";


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

#define PLAY_STATS_TOP_MAX 4

static flip_game_stats_summary_t play_stats_summary;
static flip_game_stats_t play_stats_top[PLAY_STATS_TOP_MAX];
static size_t play_stats_top_count = 0;
static flip_game_stats_t last_played_stats;
static int last_played_stats_valid = 0;

static rom_entry_t details_rom;
static screen_t details_return_screen = SCREEN_HOME;

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
    "CONTINUE",
    "LIBRARY",
    "SWITCHER",
    "PLAY STATS",
    "SETTINGS"
};

#define HOME_ITEM_COUNT \
    ((int)(sizeof(home_items) / sizeof(home_items[0])))

/*
 * Home is authored against the Miyoo Flip's native 640x480 framebuffer.
 * Keep these as explicit geometry rather than eyeballed draw coordinates:
 * architecture tests validate the regions remain on-screen and non-overlapping.
 */
#define HOME_SCREEN_W          640
#define HOME_SCREEN_H          480
#define HOME_MARGIN_X           24
#define HOME_CONTENT_W         592
#define HOME_HERO_X             24
#define HOME_HERO_Y             68
#define HOME_HERO_W            592
#define HOME_HERO_H            210
#define HOME_RECENT_LABEL_Y    292
#define HOME_RECENT_Y          310
#define HOME_RECENT_H           76
#define HOME_RECENT_CARD_W     192
#define HOME_RECENT_GAP          8
#define HOME_NAV_Y             399
#define HOME_NAV_H              36
#define HOME_NAV_W             142
#define HOME_NAV_GAP             8
#define HOME_MESSAGE_Y         440
#define HOME_FOOTER_Y          458

/*
 * GameSwitcher uses the same 24 px safe margin and fixed 640x480 canvas as
 * Home. The selected game owns the large hero card; the lower strip gives
 * context without turning the screen back into a dense utility list.
 */
#define SWITCHER_SCREEN_W          640
#define SWITCHER_SCREEN_H          480
#define SWITCHER_MARGIN_X           24
#define SWITCHER_CONTENT_W         592
#define SWITCHER_HERO_X             24
#define SWITCHER_HERO_Y             72
#define SWITCHER_HERO_W            592
#define SWITCHER_HERO_H            190
#define SWITCHER_STRIP_LABEL_Y     278
#define SWITCHER_CARD_Y            296
#define SWITCHER_CARD_H             96
#define SWITCHER_CARD_W            192
#define SWITCHER_CARD_GAP            8
#define SWITCHER_MESSAGE_Y         405
#define SWITCHER_FOOTER_Y          458


static const char *settings_items[] = {
    "CONNECTIVITY",
    "SYSTEM",
    "INTERFACE"
};

#define SETTINGS_ITEM_COUNT \
    ((int)(sizeof(settings_items) / sizeof(settings_items[0])))

static const char *connectivity_items[] = {
    "WI-FI"
};
#define CONNECTIVITY_ITEM_COUNT \
    ((int)(sizeof(connectivity_items) / sizeof(connectivity_items[0])))

static const char *system_settings_items[] = {
    "CPU OPTIMIZATION",
    "SYSTEM UPDATE",
    "DATE & TIME",
    "BIOS VERIFIER"
};
#define SYSTEM_SETTINGS_ITEM_COUNT \
    ((int)(sizeof(system_settings_items) / sizeof(system_settings_items[0])))

static const char *interface_items[] = {
    "BATTERY INDICATOR"
};
#define INTERFACE_ITEM_COUNT \
    ((int)(sizeof(interface_items) / sizeof(interface_items[0])))

static const char *optimizer_actions[] = {
    "OPTIMIZE CPU",
    "VIEW RESULTS",
    "RUN STABILITY TEST",
    "RESTORE DEFAULT SETTINGS",
    "BACK"
};

static const unsigned optimizer_validation_minutes[] = {
    5U, 10U, 20U, 30U, 60U
};

static const char *optimizer_validation_labels[] = {
    "QUICK - 5 MIN",
    "STANDARD - 10 MIN",
    "THOROUGH - 20 MIN",
    "EXTENDED - 30 MIN",
    "FULL - 60 MIN"
};

#define OPTIMIZER_VALIDATION_DURATION_COUNT \
    ((int)(sizeof(optimizer_validation_minutes) / \
        sizeof(optimizer_validation_minutes[0])))

static int optimizer_validation_selecting = 0;
static int optimizer_validation_duration_index = 1;

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
#define OPTIMIZER_VALIDATION_REQUEST_PATH \
    FLIP_STATE_DATA_ROOT "/optimizer/validation-request"
#define OPTIMIZER_VALIDATION_RESULT_PATH \
    FLIP_STATE_DATA_ROOT "/optimizer/validation-result"
#define OPTIMIZER_VALIDATION_RETURN_PATH \
    FLIP_STATE_DATA_ROOT "/optimizer/validation-return"
#define OPTIMIZER_RUNTIME_QUARANTINE_PATH \
    FLIP_STATE_DATA_ROOT "/optimizer/runtime-profile-quarantined"

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
    (void)system;

    char cleaned[512];
    const char *source = filename;
    if (flip_library_clean_display_title(
            filename,
            cleaned,
            sizeof(cleaned)) == 0) {
        source = cleaned;
    }

    size_t pos = 0;
    for (size_t i = 0;
         source[i] && pos + 1 < out_size;
         ++i) {
        char c = (char)toupper((unsigned char)source[i]);
        if ((c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == ' ' || c == '-') {
            out[pos++] = c;
        } else {
            out[pos++] = ' ';
        }
    }
    while (pos > 0 && out[pos - 1] == ' ')
        --pos;
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
            "FRONTEND_ROM index=%d display=%s path=%s\n",
            i,
            roms[i].display,
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

static void power_off_low_battery(
    flip_drm_t *drm)
{
    fprintf(
        stderr,
        "FRONTEND: critical battery poweroff requested\n");

    if (drm) {
        flip_osd_show_status(
            "BATTERY CRITICAL - SHUTTING DOWN");
        flip_osd_draw(drm);
        flip_drm_present(drm);
        usleep(500000);
    }

    if (access(FLIP_DEV_SUPPRESS_LOW_BATTERY_POWEROFF, F_OK) == 0) {
        fprintf(
            stderr,
            "FRONTEND: development low-battery poweroff suppressed\n");
        flip_osd_show_status("DEV: LOW-BATTERY POWEROFF SUPPRESSED");
        return;
    }

    power_off_now();
}

static const char *screen_name(screen_t screen)
{
    switch (screen) {
    case SCREEN_HOME: return "HOME";
    case SCREEN_SYSTEMS: return "SYSTEMS";
    case SCREEN_GAMES: return "GAMES";
    case SCREEN_RECENT: return "GAMESWITCHER";
    case SCREEN_STATS: return "PLAY_STATS";
    case SCREEN_GAME_DETAILS: return "GAME_DETAILS";
    case SCREEN_SETTINGS: return "SETTINGS";
    case SCREEN_SETTINGS_CONNECTIVITY: return "SETTINGS_CONNECTIVITY";
    case SCREEN_SETTINGS_SYSTEM: return "SETTINGS_SYSTEM";
    case SCREEN_SETTINGS_TIME: return "SETTINGS_TIME";
    case SCREEN_SETTINGS_BIOS: return "SETTINGS_BIOS";
    case SCREEN_SETTINGS_INTERFACE: return "SETTINGS_INTERFACE";
    case SCREEN_WIFI: return "WIFI";
    case SCREEN_WIFI_PASSWORD: return "WIFI_PASSWORD";
    case SCREEN_FIRST_BOOT_WELCOME: return "FIRST_BOOT_WELCOME";
    case SCREEN_FIRST_BOOT_WIFI: return "FIRST_BOOT_WIFI";
    case SCREEN_FIRST_BOOT_TIME: return "FIRST_BOOT_TIME";
    case SCREEN_FIRST_BOOT_PREFS: return "FIRST_BOOT_PREFS";
    case SCREEN_FIRST_BOOT_BIOS: return "FIRST_BOOT_BIOS";
    case SCREEN_FIRST_BOOT_READY: return "FIRST_BOOT_READY";
    case SCREEN_UPDATE: return "UPDATE";
    case SCREEN_OPTIMIZER: return "OPTIMIZER";
    case SCREEN_OPTIMIZER_RESULTS: return "OPTIMIZER_RESULTS";
    default: return "UNKNOWN";
    }
}

static void dev_publish_screen_state(screen_t screen)
{
    FILE *f = fopen(FLIP_DEV_SCREEN_STATE, "w");
    if (!f)
        return;
    fprintf(f, "%s\n", screen_name(screen));
    fclose(f);
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

static int ui_text_width_px(
    const char *text,
    int scale)
{
    if (!text ||
        scale <= 0)
        return 0;

    return (int)strlen(text) *
        6 *
        scale;
}

static int centred_text_x_in_rect(
    const char *text,
    int scale,
    int x,
    int width)
{
    int text_width =
        ui_text_width_px(
            text,
            scale);

    return x +
        (width - text_width) / 2;
}

static void fit_text_to_width(
    const char *source,
    char *destination,
    size_t destination_size,
    int max_width,
    int scale)
{
    if (!destination ||
        destination_size == 0)
        return;

    destination[0] = '\0';

    if (!source ||
        max_width <= 0 ||
        scale <= 0)
        return;

    int max_chars =
        max_width /
        (6 * scale);

    if (max_chars <= 0)
        return;

    size_t source_length =
        strlen(source);

    if ((int)source_length <=
        max_chars) {

        snprintf(
            destination,
            destination_size,
            "%s",
            source);

        return;
    }

    if (max_chars <= 3) {
        snprintf(
            destination,
            destination_size,
            "%.*s",
            max_chars,
            source);

        return;
    }

    snprintf(
        destination,
        destination_size,
        "%.*s...",
        max_chars - 3,
        source);
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

typedef struct {
    char status[48];
    unsigned calibrated_epoch;
    unsigned tested_opps;
    unsigned seconds_per_opp;
    unsigned max_temp_mc;
} optimizer_validation_result_t;

static int optimizer_validation_load(
    const flip_optimizer_result_t *profile,
    optimizer_validation_result_t *validation)
{
    if (!profile || !validation)
        return 0;

    memset(validation, 0, sizeof(*validation));
    FILE *f = fopen(OPTIMIZER_VALIDATION_RESULT_PATH, "r");
    if (!f)
        return 0;

    char line[160];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "status=", 7) == 0) {
            char *value = line + 7;
            char *end = strpbrk(value, "\r\n");
            if (end) *end = '\0';
            (void)copy_string(
                validation->status,
                sizeof(validation->status),
                value);
        } else if (sscanf(
                       line,
                       "calibrated_epoch=%u",
                       &validation->calibrated_epoch) == 1) {
        } else if (sscanf(
                       line,
                       "tested_opps=%u",
                       &validation->tested_opps) == 1) {
        } else if (sscanf(
                       line,
                       "seconds_per_opp=%u",
                       &validation->seconds_per_opp) == 1) {
        } else {
            (void)sscanf(
                line,
                "max_temp_mc=%u",
                &validation->max_temp_mc);
        }
    }

    fclose(f);
    return validation->status[0] &&
        validation->calibrated_epoch == profile->calibrated_epoch;
}

static int optimizer_write_simple_marker(
    const char *path,
    const char *text)
{
    FILE *f = fopen(path, "w");
    if (!f)
        return -1;

    int failed = fprintf(f, "%s\n", text) < 0;
    if (!failed && fflush(f) != 0)
        failed = 1;
    if (!failed && fsync(fileno(f)) != 0)
        failed = 1;
    if (fclose(f) != 0)
        failed = 1;
    return failed ? -1 : 0;
}

static int optimizer_start_validation(unsigned duration_minutes)
{
    flip_optimizer_result_t result;
    int battery = optimizer_battery_percent();

    if (duration_minutes != 5U &&
        duration_minutes != 10U &&
        duration_minutes != 20U &&
        duration_minutes != 30U &&
        duration_minutes != 60U)
        return -1;

    if (!optimizer_result_load(&result))
        return 2;
    if (access(OPTIMIZER_RUNTIME_QUARANTINE_PATH, F_OK) == 0)
        return 3;
    if (!optimizer_external_power_online() || battery < 30)
        return 1;

    (void)unlink(OPTIMIZER_VALIDATION_RESULT_PATH);

    char marker[128];
    snprintf(
        marker,
        sizeof(marker),
        "calibrated_epoch=%u\nduration_minutes=%u\nretry_count=0",
        result.calibrated_epoch,
        duration_minutes);

    if (optimizer_write_simple_marker(
            OPTIMIZER_VALIDATION_REQUEST_PATH,
            marker) < 0 ||
        optimizer_write_simple_marker(
            OPTIMIZER_VALIDATION_RETURN_PATH,
            marker) < 0) {
        (void)unlink(OPTIMIZER_VALIDATION_REQUEST_PATH);
        (void)unlink(OPTIMIZER_VALIDATION_RETURN_PATH);
        return -1;
    }

    sync();
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
    (void)unlink(OPTIMIZER_VALIDATION_REQUEST_PATH);
    (void)unlink(OPTIMIZER_VALIDATION_RESULT_PATH);
    (void)unlink(OPTIMIZER_VALIDATION_RETURN_PATH);
    (void)unlink(OPTIMIZER_RUNTIME_QUARANTINE_PATH);

    sync();
    return 0;
}

static void draw_topbar_version_indicator(flip_drm_t *drm)
{
    if (!drm)
        return;

    const int x = 12;
    const int y = 13;

    /*
     * Keep the top-left quiet during normal use: show the installed version.
     * Once a checked GitHub release is staged and differs from the installed
     * version, replace it with a simple download arrow.
     */
    if (access(FLIP_UPDATE_AVAILABLE, F_OK) == 0) {
        flip_draw_rect(drm, x + 5, y, 3, 10, FG);
        flip_draw_rect(drm, x + 1, y + 7, 11, 3, FG);
        flip_draw_rect(drm, x + 3, y + 10, 7, 3, FG);
        flip_draw_rect(drm, x + 5, y + 13, 3, 3, FG);
        return;
    }

    char version[32] = "DEV";
    FILE *f = fopen(FLIP_VERSION_FILE, "r");
    if (f) {
        if (fgets(version, sizeof(version), f)) {
            version[strcspn(version, "\r\n")] = '\0';
            if (!version[0])
                snprintf(version, sizeof(version), "DEV");
        }
        fclose(f);
    }

    flip_draw_text(drm, x, y + 1, version, 1, DIM);
}

static void draw_topbar_wifi_indicator(flip_drm_t *drm)
{
    if (!drm || !wifi_status.interface_present)
        return;

    const int x =
        flip_settings_get_battery_percent_visible() ? 552 : 581;
    const int y = 13;
    static const int heights[4] = {4, 7, 11, 15};

    if (!wifi_status.connected) {
        for (int i = 0; i < 4; ++i)
            flip_draw_rect_outline(
                drm,
                x + i * 6,
                y + 15 - heights[i],
                4,
                heights[i],
                1,
                DIM);
        return;
    }

    int level = wifi_topbar_signal_level;
    if (level < 0)
        level = 2;

    for (int i = 0; i < 4; ++i) {
        uint32_t color =
            i <= level && wifi_status.usable_ip ? FG : DIM;
        flip_draw_rect(
            drm,
            x + i * 6,
            y + 15 - heights[i],
            4,
            heights[i],
            color);
    }

    if (!wifi_status.usable_ip)
        flip_draw_text(drm, x + 23, y + 1, "!", 1, FG);
}

static void draw_battery_indicator(flip_drm_t *drm)
{
    flip_battery_snapshot_t battery;

    draw_topbar_version_indicator(drm);
    draw_topbar_wifi_indicator(drm);

    if (!drm ||
        flip_battery_read(&battery) < 0 ||
        !battery.percent_valid)
        return;

    int percent = battery.percent;
    if (percent < 0)
        percent = 0;
    if (percent > 100)
        percent = 100;

    /*
     * Compact classic battery: level is conveyed by the fill, while the exact
     * percentage sits outside the shell and can be hidden independently.
     */
    const int icon_x =
        flip_settings_get_battery_percent_visible() ? 582 : 611;
    const int icon_y = 15;
    const int icon_w = 23;
    const int icon_h = 13;
    const int inner_w = icon_w - 4;
    int fill_w = (inner_w * percent + 99) / 100;

    flip_draw_rect_outline(
        drm,
        icon_x,
        icon_y,
        icon_w,
        icon_h,
        2,
        FG);

    flip_draw_rect(
        drm,
        icon_x + icon_w,
        icon_y + 4,
        3,
        5,
        FG);

    if (fill_w > 0) {
        flip_draw_rect(
            drm,
            icon_x + 2,
            icon_y + 2,
            fill_w,
            icon_h - 4,
            FG);
    }

    if (battery.status_valid &&
        battery.status == FLIP_BATTERY_STATUS_CHARGING) {
        /* Invert the bolt when it sits over the filled portion. */
        uint32_t bolt_color = percent >= 50 ? BG : FG;
        int bx = icon_x + 8;
        int by = icon_y + 2;
        flip_draw_rect(drm, bx + 2, by, 2, 3, bolt_color);
        flip_draw_rect(drm, bx, by + 2, 4, 2, bolt_color);
        flip_draw_rect(drm, bx + 2, by + 4, 2, 3, bolt_color);
        flip_draw_rect(drm, bx + 1, by + 6, 1, 2, bolt_color);
    }

    if (flip_settings_get_battery_percent_visible()) {
        char text[16];
        snprintf(text, sizeof(text), "%d%%", percent);
        flip_draw_text(
            drm,
            icon_x + icon_w + 7,
            icon_y + 2,
            text,
            1,
            FG);
    }
}

static void clear_wifi_password(void)
{
    flip_keyboard_clear_secret(
        wifi_password,
        sizeof(wifi_password));
}

static int first_boot_screen(screen_t screen)
{
    return screen == SCREEN_FIRST_BOOT_WELCOME ||
        screen == SCREEN_FIRST_BOOT_WIFI ||
        screen == SCREEN_FIRST_BOOT_TIME ||
        screen == SCREEN_FIRST_BOOT_PREFS ||
        screen == SCREEN_FIRST_BOOT_BIOS ||
        screen == SCREEN_FIRST_BOOT_READY;
}

static const char *bios_status_text(
    flip_bios_status_t status);

static void draw_first_boot(
    flip_drm_t *drm,
    screen_t screen,
    double poweroff_progress)
{
    flip_draw_clear(drm, BG);

    const char *title = "WELCOME TO FLIPOS";
    const char *line1 = "BUILT FOR THE MIYOO FLIP";
    const char *line2 = "A BEGIN";

    if (screen == SCREEN_FIRST_BOOT_WIFI) {
        title = "WI-FI SETUP";
        line1 = wifi_status.connected
            ? "WI-FI IS CONNECTED"
            : "WI-FI IS OPTIONAL";
        line2 = wifi_status.connected
            ? "A WI-FI SETTINGS   START CONTINUE"
            : "A SET UP WI-FI   START SKIP";
    } else if (screen == SCREEN_FIRST_BOOT_TIME) {
        title = "DATE & TIME";
        line1 = time_status.timezone[0]
            ? time_status.timezone
            : "UTC";
        line2 = "LEFT/RIGHT ZONE   X SYNC   A CONTINUE";
    } else if (screen == SCREEN_FIRST_BOOT_PREFS) {
        title = "CONTROLS & DISPLAY";
        line1 = "SET A COMFORTABLE VOLUME AND BRIGHTNESS";
        line2 = "A CONTINUE";
    } else if (screen == SCREEN_FIRST_BOOT_BIOS) {
        title = "BIOS FILES";
        line1 = "OPTIONAL - USER SUPPLIED FILES";
        line2 = "A CONTINUE";
    } else if (screen == SCREEN_FIRST_BOOT_READY) {
        title = "READY";
        line1 = "FLIPOS SETUP IS COMPLETE";
        line2 = "A START FLIPOS";
    }

    flip_draw_text(
        drm,
        centred_text_x(title, UI_SCALE_HERO),
        52,
        title,
        UI_SCALE_HERO,
        FG);

    if (screen == SCREEN_FIRST_BOOT_WELCOME) {
        flip_draw_text(
            drm,
            centred_text_x("FAST. SIMPLE. CONSOLE-LIKE.", UI_SCALE_SECTION),
            146,
            "FAST. SIMPLE. CONSOLE-LIKE.",
            UI_SCALE_SECTION,
            DIM);
        flip_draw_text(
            drm,
            centred_text_x("THIS SHORT SETUP GETS THE BASICS READY.", UI_SCALE_BODY),
            205,
            "THIS SHORT SETUP GETS THE BASICS READY.",
            UI_SCALE_BODY,
            DIM);
    } else if (screen == SCREEN_FIRST_BOOT_WIFI) {
        (void)flip_wifi_get_status(&wifi_status);
        if (wifi_status.connected) {
            char wifi_line[160];
            snprintf(
                wifi_line,
                sizeof(wifi_line),
                "%s%s%s",
                wifi_status.ssid[0] ? wifi_status.ssid : "NETWORK",
                wifi_status.usable_ip ? "   IP " : "",
                wifi_status.usable_ip ? wifi_status.ip_address : "");
            flip_draw_text(
                drm,
                centred_text_x(wifi_line, UI_SCALE_BODY),
                190,
                wifi_line,
                UI_SCALE_BODY,
                FG);
        } else {
            flip_draw_text(
                drm,
                centred_text_x("YOU CAN CHANGE THIS LATER IN SETTINGS.", UI_SCALE_BODY),
                190,
                "YOU CAN CHANGE THIS LATER IN SETTINGS.",
                UI_SCALE_BODY,
                DIM);
        }
    } else if (screen == SCREEN_FIRST_BOOT_TIME) {
        char now_text[64] = "TIME UNAVAILABLE";
        (void)flip_time_format_local(now_text, sizeof(now_text));
        flip_draw_text(
            drm,
            centred_text_x(now_text, UI_SCALE_SECTION),
            178,
            now_text,
            UI_SCALE_SECTION,
            FG);
        flip_draw_text(
            drm,
            centred_text_x(
                time_status.automatic
                    ? "AUTOMATIC TIME ON"
                    : "AUTOMATIC TIME OFF",
                UI_SCALE_BODY),
            228,
            time_status.automatic
                ? "AUTOMATIC TIME ON"
                : "AUTOMATIC TIME OFF",
            UI_SCALE_BODY,
            DIM);
        flip_draw_text(
            drm,
            centred_text_x(
                wifi_status.connected
                    ? "WI-FI AVAILABLE FOR TIME SYNC"
                    : "NO WI-FI - YOU CAN SET TIME MANUALLY LATER",
                UI_SCALE_BODY),
            258,
            wifi_status.connected
                ? "WI-FI AVAILABLE FOR TIME SYNC"
                : "NO WI-FI - YOU CAN SET TIME MANUALLY LATER",
            UI_SCALE_BODY,
            DIM);
    } else if (screen == SCREEN_FIRST_BOOT_PREFS) {
        char values[96];
        snprintf(
            values,
            sizeof(values),
            "VOLUME %d%%   BRIGHTNESS %d%%",
            flip_settings_get_volume(),
            flip_power_brightness_to_percent(
                flip_settings_get_brightness()));
        flip_draw_text(
            drm,
            centred_text_x(values, UI_SCALE_SECTION),
            170,
            values,
            UI_SCALE_SECTION,
            FG);
        flip_draw_text(
            drm,
            centred_text_x("VOL -/+   VOLUME", UI_SCALE_BODY),
            234,
            "VOL -/+   VOLUME",
            UI_SCALE_BODY,
            DIM);
        flip_draw_text(
            drm,
            centred_text_x("MENU + VOL -/+   BRIGHTNESS", UI_SCALE_BODY),
            260,
            "MENU + VOL -/+   BRIGHTNESS",
            UI_SCALE_BODY,
            DIM);
    } else if (screen == SCREEN_FIRST_BOOT_BIOS) {
        static const struct {
            flip_bios_kind_t kind;
            const char *label;
        } bios_rows[] = {
            { FLIP_BIOS_GB_DMG, "GAME BOY" },
            { FLIP_BIOS_GB_CGB, "GAME BOY COLOR" },
            { FLIP_BIOS_GBA, "GAME BOY ADVANCE" },
        };

        for (size_t i = 0;
             i < sizeof(bios_rows) / sizeof(bios_rows[0]);
             ++i) {

            flip_bios_info_t info;
            memset(&info, 0, sizeof(info));

            const char *status = "ERROR";
            if (flip_bios_query(
                    bios_rows[i].kind,
                    &info) == 0) {
                status =
                    bios_status_text(info.status);
            }

            int y = 145 + (int)i * 52;

            flip_draw_text(
                drm,
                120,
                y,
                bios_rows[i].label,
                UI_SCALE_SECTION,
                FG);

            flip_draw_text(
                drm,
                440,
                y,
                status,
                UI_SCALE_SECTION,
                info.status == FLIP_BIOS_VALID
                    ? FG
                    : DIM);
        }

        flip_draw_text(
            drm,
            centred_text_x(
                "BIOS VERIFIER IS IN SETTINGS > SYSTEM",
                UI_SCALE_BODY),
            286,
            "BIOS VERIFIER IS IN SETTINGS > SYSTEM",
            UI_SCALE_BODY,
            DIM);

        flip_draw_text(
            drm,
            centred_text_x(
                "MISSING BIOS DOES NOT BLOCK SETUP",
                UI_SCALE_BODY),
            306,
            "MISSING BIOS DOES NOT BLOCK SETUP",
            UI_SCALE_BODY,
            DIM);

    } else if (screen == SCREEN_FIRST_BOOT_READY) {
        flip_draw_text(
            drm,
            centred_text_x("GAMES. GAMESWITCHER. SETTINGS.", UI_SCALE_BODY),
            190,
            "GAMES. GAMESWITCHER. SETTINGS.",
            UI_SCALE_BODY,
            DIM);
        flip_draw_text(
            drm,
            centred_text_x("YOU CAN CHANGE WI-FI AND SYSTEM OPTIONS LATER.", UI_SCALE_BODY),
            222,
            "YOU CAN CHANGE WI-FI AND SYSTEM OPTIONS LATER.",
            UI_SCALE_BODY,
            DIM);
    }

    if (screen == SCREEN_FIRST_BOOT_TIME &&
        time_message[0]) {
        flip_draw_text(
            drm,
            centred_text_x(time_message, UI_SCALE_BODY),
            292,
            time_message,
            UI_SCALE_BODY,
            FG);
    }

    flip_draw_text(
        drm,
        centred_text_x(line1, UI_SCALE_BODY),
        324,
        line1,
        UI_SCALE_BODY,
        DIM);
    flip_draw_text(
        drm,
        centred_text_x(line2, UI_SCALE_HINT),
        UI_FOOTER_Y - 4,
        line2,
        UI_SCALE_HINT,
        FG);

    draw_battery_indicator(drm);
    flip_osd_set_poweroff_progress(poweroff_progress);
    flip_osd_draw(drm);
    flip_drm_present(drm);
}

static void refresh_topbar_wifi_signal(void);

static int refresh_wifi_networks(void)
{
    memset(&wifi_status, 0, sizeof(wifi_status));
    (void)flip_wifi_get_status(&wifi_status);

    wifi_network_count = flip_wifi_scan(
        wifi_networks,
        FLIP_WIFI_MAX_NETWORKS);

    if (wifi_network_count < 0) {
        wifi_network_count = 0;
        snprintf(wifi_message, sizeof(wifi_message),
            "WI-FI SCAN FAILED");
        return -1;
    }

    if (wifi_network_count == 0) {
        snprintf(wifi_message, sizeof(wifi_message),
            "NO NETWORKS FOUND - X TO RESCAN");
    } else {
        wifi_message[0] = '\0';
    }

    (void)flip_wifi_get_status(&wifi_status);
    refresh_topbar_wifi_signal();
    return 0;
}

static int wifi_signal_level(int signal_dbm)
{
    if (signal_dbm >= -55)
        return 3;
    if (signal_dbm >= -67)
        return 2;
    if (signal_dbm >= -78)
        return 1;
    return 0;
}

static void refresh_topbar_wifi_signal(void)
{
    wifi_topbar_signal_level = -1;

    if (!wifi_status.connected || !wifi_status.ssid[0])
        return;

    for (int i = 0; i < wifi_network_count; ++i) {
        if (strcmp(wifi_networks[i].ssid, wifi_status.ssid) == 0) {
            wifi_topbar_signal_level =
                wifi_signal_level(wifi_networks[i].signal_dbm);
            return;
        }
    }
}

static void draw_wifi_lock_icon(
    flip_drm_t *drm,
    int x,
    int y,
    uint32_t color)
{
    flip_draw_rect_outline(drm, x + 2, y, 10, 8, 2, color);
    flip_draw_rect(drm, x, y + 7, 14, 11, color);
    flip_draw_rect(drm, x + 6, y + 10, 2, 5, BG);
}

static void draw_wifi(
    flip_drm_t *drm,
    int selected,
    double poweroff_progress)
{
    flip_draw_clear(drm, BG);

    flip_draw_text(
        drm,
        centred_text_x("WI-FI", UI_SCALE_TITLE),
        28,
        "WI-FI",
        UI_SCALE_TITLE,
        FG);

    char status_text[128];
    if (!wifi_status.interface_present) {
        snprintf(status_text, sizeof(status_text),
            "WI-FI HARDWARE NOT AVAILABLE");
    } else if (wifi_status.connected && wifi_status.usable_ip) {
        snprintf(status_text, sizeof(status_text),
            "CONNECTED: %s",
            wifi_status.ssid[0] ? wifi_status.ssid : "NETWORK");
    } else if (wifi_status.connected) {
        snprintf(status_text, sizeof(status_text),
            "CONNECTED - WAITING FOR NETWORK ADDRESS");
    } else {
        snprintf(status_text, sizeof(status_text),
            "NOT CONNECTED");
    }

    flip_draw_text(
        drm,
        centred_text_x(status_text, UI_SCALE_META),
        84,
        status_text,
        UI_SCALE_META,
        wifi_status.connected ? FG : DIM);

    if (wifi_status.usable_ip && wifi_status.ip_address[0]) {
        char ip_text[96];
        snprintf(ip_text, sizeof(ip_text), "IP %s", wifi_status.ip_address);
        flip_draw_text(
            drm,
            centred_text_x(ip_text, UI_SCALE_META),
            104,
            ip_text,
            UI_SCALE_META,
            DIM);
    }

    int visible = wifi_network_count < 7 ? wifi_network_count : 7;
    int first = 0;
    if (selected >= 7)
        first = selected - 6;
    if (first + visible > wifi_network_count)
        first = wifi_network_count - visible;
    if (first < 0)
        first = 0;

    for (int row = 0; row < visible; ++row) {
        int index = first + row;
        int y = 132 + row * 39;
        const flip_wifi_network_t *network = &wifi_networks[index];

        if (index == selected) {
            flip_draw_rect(drm, 60, y - 8, 520, 31, PANEL);
            flip_draw_rect_outline(drm, 60, y - 8, 520, 31, 1, FG);
        }

        uint32_t row_color = index == selected ? FG : DIM;

        flip_draw_text(
            drm,
            76,
            y,
            network->ssid,
            UI_SCALE_BODY,
            row_color);

        if (network->secure)
            draw_wifi_lock_icon(drm, 540, y - 5, row_color);

        if (wifi_status.connected &&
            strcmp(network->ssid, wifi_status.ssid) == 0) {
            flip_draw_text(
                drm,
                476,
                y,
                wifi_status.usable_ip ? "ON" : "!",
                1,
                FG);
        }
    }

    if (wifi_network_count == 0) {
        flip_draw_text(
            drm,
            centred_text_x("NO NETWORKS", 2),
            210,
            "NO NETWORKS",
            2,
            DIM);
    }

    if (wifi_message[0]) {
        flip_draw_text(
            drm,
            centred_text_x(wifi_message, UI_SCALE_BODY),
            412,
            wifi_message,
            UI_SCALE_BODY,
            FG);
    }

    const char *wifi_footer =
        wifi_return_screen == SCREEN_FIRST_BOOT_WIFI
            ? "A CONNECT   X RESCAN   START SKIP"
            : "A CONNECT   X RESCAN   Y FORGET   B BACK";

    flip_draw_text(
        drm,
        centred_text_x(wifi_footer, UI_SCALE_HINT),
        UI_FOOTER_Y,
        wifi_footer,
        UI_SCALE_HINT,
        DIM);

    draw_battery_indicator(drm);
    flip_osd_set_poweroff_progress(poweroff_progress);
    flip_osd_draw(drm);
    flip_drm_present(drm);
}

static void draw_wifi_password(
    flip_drm_t *drm,
    double poweroff_progress)
{
    flip_draw_clear(drm, BG);

    flip_draw_text(
        drm,
        centred_text_x("WI-FI PASSWORD", UI_SCALE_TITLE),
        24,
        "WI-FI PASSWORD",
        UI_SCALE_TITLE,
        FG);

    const char *ssid =
        (wifi_network_count > 0 &&
         wifi_selected >= 0 &&
         wifi_selected < wifi_network_count)
            ? wifi_networks[wifi_selected].ssid
            : "NETWORK";

    char network_text[96];
    snprintf(network_text, sizeof(network_text), "NETWORK: %s", ssid);
    flip_draw_text(
        drm,
        centred_text_x(network_text, UI_SCALE_META),
        70,
        network_text,
        UI_SCALE_META,
        DIM);

    char shown_password[72];
    size_t password_length = strlen(wifi_password);
    size_t visible_length = password_length;
    if (visible_length > sizeof(shown_password) - 1)
        visible_length = sizeof(shown_password) - 1;

    if (wifi_keyboard.reveal_text) {
        memcpy(shown_password, wifi_password, visible_length);
    } else {
        for (size_t i = 0; i < visible_length; ++i)
            shown_password[i] = '*';
    }
    shown_password[visible_length] = '\0';

    char password_text[96];
    snprintf(password_text, sizeof(password_text),
        "PASSWORD: %s", shown_password);
    flip_draw_text(
        drm,
        centred_text_x(password_text, UI_SCALE_BODY),
        96,
        password_text,
        UI_SCALE_BODY,
        FG);

    for (int i = 0; i < flip_keyboard_key_count(); ++i) {
        int row = i / FLIP_KEYBOARD_COLUMNS;
        int col = i % FLIP_KEYBOARD_COLUMNS;
        int x = 66 + col * 52;
        int y = 132 + row * 39;

        if (i == wifi_keyboard.key_index) {
            flip_draw_rect(drm, x - 11, y - 8, 42, 32, PANEL);
            flip_draw_rect_outline(drm, x - 11, y - 8, 42, 32, 2, FG);
        }

        char label[3] = {0};
        char key = FLIP_KEYBOARD_CHARS[i];
        if (key == ' ') {
            label[0] = '_';
        } else if (wifi_keyboard.uppercase &&
                   key >= 'a' && key <= 'z') {
            label[0] = (char)(key - 'a' + 'A');
        } else {
            label[0] = key;
        }
        flip_draw_text(
            drm,
            x,
            y,
            label,
            2,
            i == wifi_keyboard.key_index ? FG : DIM);
    }

    if (wifi_password_message[0]) {
        flip_draw_text(
            drm,
            centred_text_x(wifi_password_message, UI_SCALE_BODY),
            410,
            wifi_password_message,
            UI_SCALE_BODY,
            FG);
    }

    char keyboard_mode[96];
    snprintf(
        keyboard_mode,
        sizeof(keyboard_mode),
        "Y %s   X %s PASSWORD",
        wifi_keyboard.uppercase ? "LOWERCASE" : "UPPERCASE",
        wifi_keyboard.reveal_text ? "HIDE" : "SHOW");
    flip_draw_text(
        drm,
        centred_text_x(keyboard_mode, UI_SCALE_HINT),
        430,
        keyboard_mode,
        UI_SCALE_HINT,
        DIM);

    flip_draw_text(
        drm,
        centred_text_x("A TYPE   B DELETE/BACK   START CONNECT", UI_SCALE_HINT),
        452,
        "A TYPE   B DELETE/BACK   START CONNECT",
        UI_SCALE_HINT,
        DIM);

    draw_battery_indicator(drm);
    flip_osd_set_poweroff_progress(poweroff_progress);
    flip_osd_draw(drm);
    flip_drm_present(drm);
}

static int read_update_version(char *out, size_t out_size)
{
    if (!out || out_size == 0)
        return -1;
    out[0] = '\0';

    /*
     * A staged package is installable only when the updater explicitly marked
     * it as newer than the installed version. This prevents an already-current
     * release from being shown as "UPDATE FOUND".
     */
    if (access(FLIP_UPDATE_AVAILABLE, F_OK) != 0)
        return -1;

    FILE *f = fopen(FLIP_UPDATE_MANIFEST, "r");
    if (!f)
        return -1;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "version=", 8) != 0)
            continue;
        char *value = line + 8;
        value[strcspn(value, "\r\n")] = '\0';
        snprintf(out, out_size, "%.*s", (int)out_size - 1, value);
        fclose(f);
        return out[0] ? 0 : -1;
    }

    fclose(f);
    return -1;
}

static int run_update_installer(void)
{
    pid_t pid = fork();
    if (pid < 0)
        return -1;

    if (pid == 0) {
        char *argv[] = {
            (char *)FLIP_UPDATE_INSTALLER,
            (char *)FLIP_UPDATE_PACKAGE,
            NULL
        };
        execv(argv[0], argv);
        _exit(127);
    }

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR)
            return -1;
    }

    if (!WIFEXITED(status))
        return -1;
    return WEXITSTATUS(status) == 0 ? 0 : -1;
}

static int delete_update_package(void)
{
    pid_t pid = fork();
    if (pid < 0)
        return -1;

    if (pid == 0) {
        char *argv[] = {
            (char *)"/bin/rm",
            (char *)"-rf",
            (char *)FLIP_UPDATE_PACKAGE,
            NULL
        };
        execv(argv[0], argv);
        _exit(127);
    }

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR)
            return -1;
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        return -1;

    (void)unlink(FLIP_UPDATE_AVAILABLE);
    sync();
    return 0;
}

static int run_update_fetcher(void)
{
    pid_t pid = fork();
    if (pid < 0)
        return -1;

    if (pid == 0) {
        char *argv[] = {
            (char *)FLIP_UPDATE_FETCHER,
            NULL
        };
        execv(argv[0], argv);
        _exit(127);
    }

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR)
            return -1;
    }

    if (!WIFEXITED(status))
        return -1;
    return WEXITSTATUS(status) == 0 ? 0 : -1;
}

static void draw_update(
    flip_drm_t *drm,
    double poweroff_progress)
{
    flip_draw_clear(drm, BG);

    flip_draw_text(
        drm,
        centred_text_x("SYSTEM UPDATE", UI_SCALE_TITLE),
        34,
        "SYSTEM UPDATE",
        UI_SCALE_TITLE,
        FG);

    char version[96];
    int package_ready = read_update_version(version, sizeof(version)) == 0;

    if (update_complete) {
        flip_draw_text(
            drm,
            centred_text_x("UPDATE COMPLETE", UI_SCALE_BODY),
            150,
            "UPDATE COMPLETE",
            UI_SCALE_BODY,
            FG);
        flip_draw_text(
            drm,
            centred_text_x("RESTART TO FINISH", UI_SCALE_BODY),
            198,
            "RESTART TO FINISH",
            UI_SCALE_BODY,
            FG);
        flip_draw_text(
            drm,
            centred_text_x("A RESTART NOW   B LATER", UI_SCALE_HINT),
            278,
            "A RESTART NOW   B LATER",
            UI_SCALE_HINT,
            FG);
    } else if (update_confirm && package_ready) {
        char package_text[128];
        snprintf(package_text, sizeof(package_text),
            "INSTALL %.80s?", version);
        flip_draw_text(
            drm,
            centred_text_x(package_text, UI_SCALE_BODY),
            152,
            package_text,
            UI_SCALE_BODY,
            FG);
        flip_draw_text(
            drm,
            centred_text_x("CURRENT VERSION IS KEPT IF INSTALL FAILS", UI_SCALE_BODY),
            205,
            "CURRENT VERSION IS KEPT IF INSTALL FAILS",
            UI_SCALE_BODY,
            DIM);
        flip_draw_text(
            drm,
            centred_text_x("A CONFIRM   B CANCEL", UI_SCALE_HINT),
            276,
            "A CONFIRM   B CANCEL",
            UI_SCALE_HINT,
            FG);
    } else if (package_ready) {
        char package_text[128];
        snprintf(package_text, sizeof(package_text),
            "UPDATE FOUND: %.80s", version);
        flip_draw_text(
            drm,
            centred_text_x(package_text, UI_SCALE_BODY),
            152,
            package_text,
            UI_SCALE_BODY,
            FG);
        flip_draw_text(
            drm,
            centred_text_x("PACKAGE WILL BE VERIFIED BEFORE INSTALL", UI_SCALE_BODY),
            205,
            "PACKAGE WILL BE VERIFIED BEFORE INSTALL",
            UI_SCALE_BODY,
            DIM);
        flip_draw_text(
            drm,
            centred_text_x(
                "A INSTALL   X CHECK GITHUB   Y DELETE",
                UI_SCALE_HINT),
            276,
            "A INSTALL   X CHECK GITHUB   Y DELETE",
            UI_SCALE_HINT,
            FG);
    } else {
        flip_draw_text(
            drm,
            centred_text_x("NO UPDATE PACKAGE FOUND", UI_SCALE_BODY),
            152,
            "NO UPDATE PACKAGE FOUND",
            UI_SCALE_BODY,
            DIM);
        flip_draw_text(
            drm,
            centred_text_x("DOWNLOAD FROM THE FLIPOS GITHUB RELEASE", UI_SCALE_BODY),
            214,
            "DOWNLOAD FROM THE FLIPOS GITHUB RELEASE",
            UI_SCALE_BODY,
            DIM);
        flip_draw_text(
            drm,
            centred_text_x("A CHECK GITHUB", UI_SCALE_HINT),
            276,
            "A CHECK GITHUB",
            UI_SCALE_HINT,
            FG);
    }

    if (update_message[0]) {
        flip_draw_text(
            drm,
            centred_text_x(update_message, UI_SCALE_BODY),
            360,
            update_message,
            UI_SCALE_BODY,
            FG);
    }

    flip_draw_text(
        drm,
        centred_text_x("B BACK", UI_SCALE_HINT),
        445,
        "B BACK",
        UI_SCALE_HINT,
        DIM);

    draw_battery_indicator(drm);
    flip_osd_set_poweroff_progress(poweroff_progress);
    flip_osd_draw(drm);
    flip_drm_present(drm);
}

static void refresh_time_status(void)
{
    memset(&time_status, 0, sizeof(time_status));
    (void)flip_time_load_status(&time_status);

    time_timezone_index = 0;
    for (int i = 0; i < flip_time_timezone_count(); ++i) {
        const char *zone = flip_time_timezone_at(i);
        if (zone && strcmp(zone, time_status.timezone) == 0) {
            time_timezone_index = i;
            break;
        }
    }
}

static void change_time_zone(int delta)
{
    int count = flip_time_timezone_count();
    if (count <= 0)
        return;

    time_timezone_index += delta;
    while (time_timezone_index < 0)
        time_timezone_index += count;
    while (time_timezone_index >= count)
        time_timezone_index -= count;

    const char *zone = flip_time_timezone_at(time_timezone_index);
    if (zone && flip_time_set_timezone(zone) == 0) {
        refresh_time_status();
        snprintf(
            time_message,
            sizeof(time_message),
            "TIME ZONE SAVED");
    } else {
        snprintf(
            time_message,
            sizeof(time_message),
            "TIME ZONE COULD NOT BE SAVED");
    }
}

static void begin_manual_time_edit(void)
{
    time_t now = time(NULL);
    memset(&time_manual_value, 0, sizeof(time_manual_value));
    (void)localtime_r(&now, &time_manual_value);
    time_manual_field = 0;
    time_manual_editing = 1;
    time_message[0] = '\0';
}

static void adjust_manual_time_field(int delta)
{
    switch (time_manual_field) {
        case 0:
            time_manual_value.tm_year += delta;
            if (time_manual_value.tm_year < 100)
                time_manual_value.tm_year = 100;
            if (time_manual_value.tm_year > 199)
                time_manual_value.tm_year = 199;
            break;
        case 1:
            time_manual_value.tm_mon += delta;
            if (time_manual_value.tm_mon < 0)
                time_manual_value.tm_mon = 11;
            if (time_manual_value.tm_mon > 11)
                time_manual_value.tm_mon = 0;
            break;
        case 2:
            time_manual_value.tm_mday += delta;
            if (time_manual_value.tm_mday < 1)
                time_manual_value.tm_mday = 31;
            if (time_manual_value.tm_mday > 31)
                time_manual_value.tm_mday = 1;
            break;
        case 3:
            time_manual_value.tm_hour += delta;
            if (time_manual_value.tm_hour < 0)
                time_manual_value.tm_hour = 23;
            if (time_manual_value.tm_hour > 23)
                time_manual_value.tm_hour = 0;
            break;
        case 4:
            time_manual_value.tm_min += delta;
            if (time_manual_value.tm_min < 0)
                time_manual_value.tm_min = 59;
            if (time_manual_value.tm_min > 59)
                time_manual_value.tm_min = 0;
            break;
        default:
            break;
    }
}

static void draw_time_settings(
    flip_drm_t *drm,
    double poweroff_progress)
{
    flip_draw_clear(drm, BG);

    flip_draw_text(
        drm,
        centred_text_x("DATE & TIME", UI_SCALE_TITLE),
        28,
        "DATE & TIME",
        UI_SCALE_TITLE,
        FG);

    char now_text[64] = "TIME UNAVAILABLE";
    (void)flip_time_format_local(now_text, sizeof(now_text));
    flip_draw_text(
        drm,
        centred_text_x(now_text, UI_SCALE_SECTION),
        82,
        now_text,
        UI_SCALE_SECTION,
        FG);

    char auto_text[80];
    snprintf(
        auto_text,
        sizeof(auto_text),
        "AUTOMATIC TIME   %s",
        time_status.automatic ? "ON" : "OFF");

    char zone_text[96];
    snprintf(
        zone_text,
        sizeof(zone_text),
        "TIME ZONE   %s",
        time_status.timezone[0] ? time_status.timezone : "UTC");

    const char *rows[4] = {
        auto_text,
        zone_text,
        "SYNC TIME NOW",
        "SET DATE & TIME MANUALLY"
    };

    for (int i = 0; i < 4; ++i) {
        int y = 150 + i * 58;
        int disabled = i == 3 && time_status.automatic;
        if (i == time_selected && !time_manual_editing) {
            flip_draw_rect(drm, 70, y - 10, 500, 42, PANEL);
            flip_draw_rect_outline(drm, 70, y - 10, 500, 42, 2, FG);
        }

        int scale = strlen(rows[i]) > 24 ? UI_SCALE_BODY : UI_SCALE_SECTION;
        flip_draw_text(
            drm,
            centred_text_x(rows[i], scale),
            y,
            rows[i],
            scale,
            disabled ? DIM : (i == time_selected ? FG : DIM));
    }

    if (time_manual_editing) {
        char edit_text[96];
        snprintf(
            edit_text,
            sizeof(edit_text),
            "%04d-%02d-%02d   %02d:%02d",
            time_manual_value.tm_year + 1900,
            time_manual_value.tm_mon + 1,
            time_manual_value.tm_mday,
            time_manual_value.tm_hour,
            time_manual_value.tm_min);

        flip_draw_rect(drm, 72, 354, 496, 84, PANEL);
        flip_draw_rect_outline(drm, 72, 354, 496, 84, 2, FG);
        flip_draw_text(
            drm,
            centred_text_x(edit_text, UI_SCALE_SECTION),
            369,
            edit_text,
            UI_SCALE_SECTION,
            FG);
        flip_draw_text(
            drm,
            centred_text_x(
                "LEFT/RIGHT FIELD   UP/DOWN CHANGE",
                UI_SCALE_BODY),
            403,
            "LEFT/RIGHT FIELD   UP/DOWN CHANGE",
            UI_SCALE_BODY,
            DIM);
        flip_draw_text(
            drm,
            centred_text_x("A SAVE   B CANCEL", UI_SCALE_HINT),
            UI_FOOTER_Y,
            "A SAVE   B CANCEL",
            UI_SCALE_HINT,
            FG);
    } else {
        const char *footer =
            time_selected == 1
                ? "LEFT/RIGHT TIME ZONE   A SELECT   B BACK"
                : "A SELECT   B BACK";
        flip_draw_text(
            drm,
            centred_text_x(footer, UI_SCALE_HINT),
            UI_FOOTER_Y,
            footer,
            UI_SCALE_HINT,
            DIM);
    }

    if (time_message[0]) {
        flip_draw_text(
            drm,
            centred_text_x(time_message, UI_SCALE_BODY),
            420,
            time_message,
            UI_SCALE_BODY,
            FG);
    }

    draw_battery_indicator(drm);
    flip_osd_set_poweroff_progress(poweroff_progress);
    flip_osd_draw(drm);
    flip_drm_present(drm);
}

static void draw_settings_menu(
    flip_drm_t *drm,
    const char *title,
    const char *const *items,
    int item_count,
    int selected,
    int show_battery_mode,
    double poweroff_progress)
{
    flip_draw_clear(drm, BG);

    flip_draw_text(
        drm,
        centred_text_x(title, UI_SCALE_TITLE),
        28,
        title,
        UI_SCALE_TITLE,
        FG);

    for (int i = 0; i < item_count; ++i) {
        int y = 145 + i * 72;

        if (i == selected) {
            /*
             * Interface rows can carry a second status line (for example
             * BATTERY INDICATOR + ICON/PERCENTAGE mode). Give those rows
             * enough vertical room so the status text stays inside the
             * selection card at the larger Beta UI scale.
             */
            int selection_height =
                (show_battery_mode && i == 0) ? 72 : 54;
            flip_draw_rect(
                drm, 115, y - 12, 410, selection_height, PANEL);
            flip_draw_rect_outline(
                drm, 115, y - 12, 410, selection_height, 2, FG);
        }

        int scale = strlen(items[i]) > 16 ? UI_SCALE_BODY : UI_SCALE_SECTION;
        flip_draw_text(
            drm,
            centred_text_x(items[i], scale),
            y,
            items[i],
            scale,
            i == selected ? FG : DIM);

        if (show_battery_mode && i == 0) {
            const char *mode =
                flip_settings_get_battery_percent_visible()
                    ? "ICON + PERCENTAGE"
                    : "ICON ONLY";
            flip_draw_text(
                drm,
                centred_text_x(mode, UI_SCALE_META),
                y + 30,
                mode,
                UI_SCALE_META,
                i == selected ? FG : DIM);
        }
    }

    flip_draw_text(
        drm,
        centred_text_x("A SELECT   B BACK", UI_SCALE_HINT),
        UI_FOOTER_Y,
        "A SELECT   B BACK",
        UI_SCALE_HINT,
        DIM);

    draw_battery_indicator(drm);
    flip_osd_set_poweroff_progress(poweroff_progress);
    flip_osd_draw(drm);
    flip_drm_present(drm);
}

static void draw_settings(
    flip_drm_t *drm,
    int selected,
    double poweroff_progress)
{
    flip_draw_clear(drm, BG);

    flip_draw_text(
        drm,
        centred_text_x("SETTINGS", UI_SCALE_TITLE),
        28,
        "SETTINGS",
        UI_SCALE_TITLE,
        FG);

    char quick_status[96];
    snprintf(
        quick_status,
        sizeof(quick_status),
        "VOLUME %d%%   BRIGHTNESS %d%%",
        flip_settings_get_volume(),
        flip_power_brightness_to_percent(
            flip_settings_get_brightness()));
    flip_draw_text(
        drm,
        centred_text_x(quick_status, UI_SCALE_META),
        90,
        quick_status,
        UI_SCALE_META,
        DIM);

    for (int i = 0; i < SETTINGS_ITEM_COUNT; ++i) {
        int y = 145 + i * 72;
        if (i == selected) {
            flip_draw_rect(drm, 115, y - 12, 410, 54, PANEL);
            flip_draw_rect_outline(drm, 115, y - 12, 410, 54, 2, FG);
        }
        flip_draw_text(
            drm,
            centred_text_x(settings_items[i], UI_SCALE_SECTION),
            y,
            settings_items[i],
            UI_SCALE_SECTION,
            i == selected ? FG : DIM);
    }

    flip_draw_text(
        drm,
        centred_text_x("A OPEN   B BACK", UI_SCALE_HINT),
        UI_FOOTER_Y,
        "A OPEN   B BACK",
        UI_SCALE_HINT,
        DIM);

    draw_battery_indicator(drm);
    flip_osd_set_poweroff_progress(poweroff_progress);
    flip_osd_draw(drm);
    flip_drm_present(drm);
}


static const char *bios_status_text(
    flip_bios_status_t status)
{
    switch (status) {
    case FLIP_BIOS_VALID:
        return "VALID";
    case FLIP_BIOS_MISSING:
        return "MISSING";
    case FLIP_BIOS_INVALID:
        return "INVALID";
    case FLIP_BIOS_ERROR:
    default:
        return "ERROR";
    }
}

static void draw_bios_verifier(
    flip_drm_t *drm,
    double poweroff_progress)
{
    static const struct {
        flip_bios_kind_t kind;
        const char *label;
    } rows[] = {
        { FLIP_BIOS_GB_DMG, "GAME BOY" },
        { FLIP_BIOS_GB_CGB, "GAME BOY COLOR" },
        { FLIP_BIOS_GBA, "GAME BOY ADVANCE" },
    };

    flip_draw_clear(drm, BG);

    flip_draw_text(
        drm,
        centred_text_x("BIOS VERIFIER", UI_SCALE_TITLE),
        24,
        "BIOS VERIFIER",
        UI_SCALE_TITLE,
        FG);

    flip_draw_text(
        drm,
        centred_text_x("USER-SUPPLIED BIOS FILES", UI_SCALE_META),
        76,
        "USER-SUPPLIED BIOS FILES",
        UI_SCALE_META,
        DIM);

    for (size_t i = 0;
         i < sizeof(rows) / sizeof(rows[0]);
         ++i) {

        flip_bios_info_t info;
        memset(&info, 0, sizeof(info));

        int query_result =
            flip_bios_query(
                rows[i].kind,
                &info);

        const char *status =
            query_result == 0
                ? bios_status_text(info.status)
                : "ERROR";

        int y = 125 + (int)i * 82;

        flip_draw_rect(
            drm,
            55,
            y - 10,
            530,
            62,
            PANEL);

        flip_draw_text(
            drm,
            75,
            y,
            rows[i].label,
            UI_SCALE_SECTION,
            FG);

        flip_draw_text(
            drm,
            430,
            y,
            status,
            UI_SCALE_SECTION,
            info.status == FLIP_BIOS_VALID
                ? FG
                : DIM);

        if (query_result == 0 &&
            info.filename) {

            flip_draw_text(
                drm,
                75,
                y + 30,
                info.filename,
                UI_SCALE_META,
                DIM);
        }
    }

    flip_draw_text(
        drm,
        centred_text_x("VALID = SIZE + SHA-256 MATCH", UI_SCALE_BODY),
        386,
        "VALID = SIZE + SHA-256 MATCH",
        UI_SCALE_BODY,
        DIM);

    flip_draw_text(
        drm,
        centred_text_x("A RECHECK   B BACK", UI_SCALE_HINT),
        430,
        "A RECHECK   B BACK",
        UI_SCALE_HINT,
        DIM);

    draw_battery_indicator(drm);
    flip_osd_set_poweroff_progress(poweroff_progress);
    flip_osd_draw(drm);
    flip_drm_present(drm);
}


static void draw_optimizer_results(
    flip_drm_t *drm,
    double poweroff_progress)
{
    flip_optimizer_result_t result;
    int have_result = optimizer_result_load(&result);

    flip_draw_clear(drm, BG);

    flip_draw_text(
        drm,
        centred_text_x("CPU OPTIMIZER RESULTS", UI_SCALE_TITLE),
        22,
        "CPU OPTIMIZER RESULTS",
        UI_SCALE_TITLE,
        FG);

    if (!have_result) {
        flip_draw_text(
            drm,
            centred_text_x("NO RESULTS", 4),
            180,
            "NO RESULTS",
            4,
            FG);
        flip_draw_text(
            drm,
            centred_text_x("RUN CPU OPTIMIZATION FIRST", 2),
            245,
            "RUN CPU OPTIMIZATION FIRST",
            2,
            DIM);
    } else {
        optimizer_validation_result_t validation;
        int have_validation = optimizer_validation_load(&result, &validation);
        int passed = have_validation && strcmp(validation.status, "passed") == 0;
        int cancelled = have_validation && strncmp(validation.status, "cancelled", 9) == 0;
        int quarantined = access(OPTIMIZER_RUNTIME_QUARANTINE_PATH, F_OK) == 0;

        const char *status =
            quarantined
                ? "FAILED"
                : passed
                    ? "PASSED"
                    : cancelled
                        ? "CANCELLED"
                        : have_validation
                            ? "FAILED"
                            : "TEST NEEDED";
        int status_scale = strlen(status) <= 9 ? 4 : 3;

        flip_draw_text(
            drm,
            centred_text_x(status, status_scale),
            68,
            status,
            status_scale,
            FG);

        char detail[128];
        if (passed) {
            unsigned minutes =
                (validation.seconds_per_opp * validation.tested_opps) / 60U;
            unsigned temp_c =
                (validation.max_temp_mc + 500U) / 1000U;
            snprintf(
                detail,
                sizeof(detail),
                "%u MIN STABILITY TEST   MAX %u C",
                minutes,
                temp_c);
        } else if (quarantined) {
            snprintf(detail, sizeof(detail),
                "OPTIMIZATION DISABLED - RUN AGAIN");
        } else if (cancelled) {
            snprintf(detail, sizeof(detail),
                "STABILITY TEST CANCELLED");
        } else if (have_validation) {
            snprintf(detail, sizeof(detail),
                "TEST FAILED - SAFE SETTINGS RESTORED");
        } else {
            snprintf(detail, sizeof(detail),
                "RUN A STABILITY TEST BEFORE USE");
        }

        flip_draw_text(
            drm,
            centred_text_x(detail, UI_SCALE_BODY),
            120,
            detail,
            UI_SCALE_BODY,
            passed ? FG : DIM);

        char guardband[64];
        snprintf(
            guardband,
            sizeof(guardband),
            "SAFETY MARGIN: %u mV",
            result.guardband_uv / 1000U);
        flip_draw_text(
            drm,
            centred_text_x(guardband, UI_SCALE_META),
            142,
            guardband,
            UI_SCALE_META,
            DIM);

        flip_draw_text(
            drm,
            128,
            176,
            "CPU SPEED      DEFAULT       TUNED",
            UI_SCALE_BODY,
            DIM);

        int shown = 0;
        for (size_t reverse = result.opp_count;
             reverse > 0 && shown < 5;
             --reverse) {
            const flip_optimizer_opp_t *opp = &result.opps[reverse - 1];
            if (opp->optimized_uv >= opp->stock_uv)
                continue;

            char row[96];
            snprintf(
                row,
                sizeof(row),
                "%4u MHz       %4u       %4u mV",
                opp->frequency_khz / 1000U,
                opp->stock_uv / 1000U,
                opp->optimized_uv / 1000U);
            flip_draw_text(
                drm,
                128,
                205 + shown * 38,
                row,
                2,
                FG);
            shown++;
        }

        if (shown == 0) {
            flip_draw_text(
                drm,
                centred_text_x("NO VOLTAGE REDUCTIONS RECORDED", 2),
                240,
                "NO VOLTAGE REDUCTIONS RECORDED",
                2,
                DIM);
        }
    }

    flip_draw_text(
        drm,
        centred_text_x("B BACK", UI_SCALE_HINT),
        442,
        "B BACK",
        UI_SCALE_HINT,
        DIM);

    draw_battery_indicator(
        drm);

    flip_osd_set_poweroff_progress(poweroff_progress);
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

    const char *state_text = "NOT OPTIMIZED";
    if (have_result) {
        optimizer_validation_result_t validation_state;
        int have_validation_state =
            optimizer_validation_load(
                &result,
                &validation_state);
        if (have_validation_state &&
            strcmp(validation_state.status, "passed") == 0)
            state_text = "VERIFIED";
        else if (access(
                     OPTIMIZER_RUNTIME_QUARANTINE_PATH,
                     F_OK) == 0)
            state_text = "DISABLED";
        else
            state_text = "CALIBRATED";
    }

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
        optimizer_validation_result_t validation;
        const char *profile_text = "READY FOR TEST";
        const char *profile_hint = "RUN A STABILITY TEST BEFORE USING TUNED SETTINGS";
        if (optimizer_validation_load(&result, &validation)) {
            if (strcmp(validation.status, "passed") == 0) {
                profile_text = "OPTIMIZATION VERIFIED";
                profile_hint = "STABILITY TEST PASSED - READY TO USE";
            } else if (strncmp(validation.status, "cancelled", 9) == 0) {
                profile_text = "TEST CANCELLED";
                profile_hint = "RUN THE STABILITY TEST BEFORE USE";
            } else {
                profile_text = "SAFE SETTINGS RESTORED";
                profile_hint = "RUN OPTIMIZATION AGAIN TO RETRY";
            }
        } else if (access(OPTIMIZER_RUNTIME_QUARANTINE_PATH, F_OK) == 0) {
            profile_text = "OPTIMIZATION DISABLED";
            profile_hint = "RUN OPTIMIZATION AGAIN BEFORE USE";
        }

        flip_draw_text(
            drm,
            centred_text_x(profile_text, 2),
            154,
            profile_text,
            2,
            FG);

        flip_draw_text(
            drm,
            centred_text_x(profile_hint, UI_SCALE_BODY),
            190,
            profile_hint,
            UI_SCALE_BODY,
            DIM);
    } else {
        flip_draw_text(
            drm,
            centred_text_x(
                "FINDS SAFE CPU SETTINGS FOR THIS DEVICE",
                1),
            132,
            "FINDS SAFE CPU SETTINGS FOR THIS DEVICE",
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
            label = "OPTIMIZE AGAIN";

        if (i == 1 && !have_result)
            label = "VIEW RESULTS (NONE)";
        if (i == 2 && !have_result)
            label = "STABILITY TEST (NONE)";

        int y = 278 + i * 29;

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
            centred_text_x(message, UI_SCALE_BODY),
            430,
            message,
            UI_SCALE_BODY,
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

    if (optimizer_validation_selecting) {
        flip_draw_rect(drm, 92, 318, 456, 132, PANEL);
        flip_draw_rect_outline(drm, 92, 318, 456, 132, 2, FG);
        flip_draw_text(
            drm,
            centred_text_x("CHOOSE STABILITY TEST", 2),
            333,
            "CHOOSE STABILITY TEST",
            2,
            FG);
        flip_draw_text(
            drm,
            centred_text_x(
                optimizer_validation_labels[
                    optimizer_validation_duration_index],
                2),
            372,
            optimizer_validation_labels[
                optimizer_validation_duration_index],
            2,
            FG);
        flip_draw_text(
            drm,
            centred_text_x("UP/DOWN CHANGE   A SELECT", UI_SCALE_HINT),
            413,
            "UP/DOWN CHANGE   A SELECT",
            UI_SCALE_HINT,
            DIM);
        flip_draw_text(
            drm,
            centred_text_x("B CANCEL", UI_SCALE_HINT),
            432,
            "B CANCEL",
            UI_SCALE_HINT,
            DIM);
    }

    if (confirm_action != 0) {
        const char *confirm =
            confirm_action == 1
                ? "START CPU OPTIMIZER?"
                : confirm_action == 3
                    ? "START STABILITY TEST?"
                    : "RESTORE DEFAULT CPU SETTINGS?";

        char validation_detail[96];
        snprintf(
            validation_detail,
            sizeof(validation_detail),
            "%s - KEEP CHARGER CONNECTED",
            optimizer_validation_labels[
                optimizer_validation_duration_index]);

        const char *detail =
            confirm_action == 1
                ? "KEEP CHARGER CONNECTED"
                : confirm_action == 3
                    ? validation_detail
                    : "DEFAULT CPU SETTINGS WILL BE RESTORED";

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
            centred_text_x(detail, UI_SCALE_BODY),
            394,
            detail,
            UI_SCALE_BODY,
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

    draw_battery_indicator(
        drm);

    flip_osd_set_poweroff_progress(
        poweroff_progress);
    flip_osd_draw(drm);
    flip_drm_present(drm);
}

static void refresh_play_stats(void)
{
    memset(
        &play_stats_summary,
        0,
        sizeof(play_stats_summary));

    memset(
        play_stats_top,
        0,
        sizeof(play_stats_top));

    play_stats_top_count = 0;

    if (flip_game_stats_collect(
            &play_stats_summary,
            play_stats_top,
            PLAY_STATS_TOP_MAX,
            &play_stats_top_count) < 0) {

        memset(
            &play_stats_summary,
            0,
            sizeof(play_stats_summary));

        play_stats_top_count = 0;
    }

    last_played_stats_valid = 0;
    memset(
        &last_played_stats,
        0,
        sizeof(last_played_stats));

    if (recent_count > 0 &&
        recent_roms[0].system &&
        recent_roms[0].system->id &&
        flip_game_stats_load(
            recent_roms[0].system->id,
            recent_roms[0].path,
            &last_played_stats) == 1) {

        last_played_stats_valid = 1;
    }
}

static void format_play_duration(
    unsigned long long seconds,
    char *out,
    size_t out_size)
{
    if (!out || out_size == 0)
        return;

    if (seconds < 60ULL) {
        snprintf(out, out_size, "0M");
        return;
    }

    unsigned long long minutes =
        seconds / 60ULL;

    if (minutes < 60ULL) {
        snprintf(
            out,
            out_size,
            "%lluM",
            minutes);
        return;
    }

    unsigned long long hours =
        minutes / 60ULL;

    minutes %= 60ULL;

    if (hours < 100ULL) {
        snprintf(
            out,
            out_size,
            "%lluH %02lluM",
            hours,
            minutes);
    } else {
        snprintf(
            out,
            out_size,
            "%lluH",
            hours);
    }
}

static void format_last_played(
    long long epoch,
    char *out,
    size_t out_size)
{
    if (!out || out_size == 0)
        return;

    long long now =
        (long long)time(NULL);

    if (epoch <= 0 || now <= 0) {
        snprintf(out, out_size, "NOT YET");
        return;
    }

    long long delta =
        now >= epoch
            ? now - epoch
            : 0;

    if (delta < 60) {
        snprintf(out, out_size, "JUST NOW");
    } else if (delta < 3600) {
        snprintf(
            out,
            out_size,
            "%lldM AGO",
            delta / 60);
    } else if (delta < 86400) {
        snprintf(
            out,
            out_size,
            "%lldH AGO",
            delta / 3600);
    } else if (delta < 172800) {
        snprintf(out, out_size, "YESTERDAY");
    } else if (delta < 604800) {
        snprintf(
            out,
            out_size,
            "%lld DAYS AGO",
            delta / 86400);
    } else {
        time_t when = (time_t)epoch;
        struct tm local;
        if (localtime_r(&when, &local) &&
            strftime(
                out,
                out_size,
                "%d %b",
                &local) > 0) {

            for (size_t i = 0;
                 out[i];
                 ++i) {
                out[i] =
                    (char)toupper(
                        (unsigned char)out[i]);
            }
        } else {
            snprintf(out, out_size, "EARLIER");
        }
    }
}


static void format_played_date(
    long long epoch,
    char *out,
    size_t out_size)
{
    if (!out || out_size == 0)
        return;

    if (epoch <= 0) {
        snprintf(out, out_size, "NOT YET");
        return;
    }

    time_t when = (time_t)epoch;
    struct tm local;

    if (!localtime_r(&when, &local) ||
        strftime(
            out,
            out_size,
            "%d %b %Y",
            &local) == 0) {
        snprintf(out, out_size, "UNKNOWN");
        return;
    }

    for (size_t i = 0;
         out[i];
         ++i) {
        out[i] =
            (char)toupper(
                (unsigned char)out[i]);
    }
}

static void draw_home(
    flip_drm_t *drm,
    int selected,
    const char *message,
    double poweroff_progress)
{
    flip_draw_clear(
        drm,
        BG);

    /*
     * Top bar is reserved for version, Wi-Fi and battery. The centred wordmark
     * is deliberately only 72 px wide at scale 2, so it cannot collide with
     * the status regions at either edge.
     */
    flip_draw_text(
        drm,
        centred_text_x(
            "FLIPOS",
            UI_SCALE_BODY),
        14,
        "FLIPOS",
        UI_SCALE_BODY,
        FG);

    flip_draw_text(
        drm,
        centred_text_x(
            "CONTINUE",
            UI_SCALE_META),
        44,
        "CONTINUE",
        UI_SCALE_META,
        selected == 0
            ? FG
            : DIM);

    flip_draw_rect(
        drm,
        HOME_HERO_X,
        HOME_HERO_Y,
        HOME_HERO_W,
        HOME_HERO_H,
        PANEL);

    flip_draw_rect_outline(
        drm,
        HOME_HERO_X,
        HOME_HERO_Y,
        HOME_HERO_W,
        HOME_HERO_H,
        selected == 0 ? 2 : 1,
        selected == 0
            ? FG
            : DIM);

    if (recent_count > 0) {
        const rom_entry_t *recent =
            &recent_roms[0];

        const char *system_name =
            recent->system &&
            recent->system->display_name
                ? recent->system->display_name
                : "GAME";

        int can_resume =
            flip_system_resume_available(
                recent->system,
                recent->path);

        char system_visible[64];

        fit_text_to_width(
            system_name,
            system_visible,
            sizeof(system_visible),
            HOME_HERO_W - 48,
            1);

        flip_draw_text(
            drm,
            centred_text_x_in_rect(
                system_visible,
                1,
                HOME_HERO_X,
                HOME_HERO_W),
            87,
            system_visible,
            1,
            DIM);

        int title_scale =
            UI_SCALE_SECTION;

        while (title_scale > 1 &&
               ui_text_width_px(
                   recent->display,
                   title_scale) >
                   HOME_HERO_W - 64) {

            title_scale--;
        }

        char game_name[96];

        fit_text_to_width(
            recent->display,
            game_name,
            sizeof(game_name),
            HOME_HERO_W - 64,
            title_scale);

        flip_draw_text(
            drm,
            centred_text_x_in_rect(
                game_name,
                title_scale,
                HOME_HERO_X,
                HOME_HERO_W),
            111,
            game_name,
            title_scale,
            FG);

        flip_draw_text(
            drm,
            centred_text_x_in_rect(
                can_resume
                    ? "RESUME READY"
                    : "LAST PLAYED GAME",
                1,
                HOME_HERO_X,
                HOME_HERO_W),
            149,
            can_resume
                ? "RESUME READY"
                : "LAST PLAYED GAME",
            1,
            can_resume
                ? FG
                : DIM);

        flip_draw_rect(
            drm,
            HOME_HERO_X + 32,
            172,
            HOME_HERO_W - 64,
            1,
            DIM);

        char played[32] = "0M";
        char launches[24] = "0";
        char last_played[32] = "NOT YET";

        if (last_played_stats_valid) {
            format_play_duration(
                last_played_stats.total_play_seconds,
                played,
                sizeof(played));

            snprintf(
                launches,
                sizeof(launches),
                "%llu",
                last_played_stats.launches);

            format_last_played(
                last_played_stats.last_played_epoch,
                last_played,
                sizeof(last_played));
        }

        static const int stat_center[3] = {
            144, 320, 496
        };

        static const char *stat_label[3] = {
            "PLAY TIME",
            "LAUNCHES",
            "LAST PLAYED"
        };

        const char *stat_value[3] = {
            played,
            launches,
            last_played
        };

        for (int i = 0;
             i < 3;
             ++i) {

            int label_width =
                ui_text_width_px(
                    stat_label[i],
                    1);

            flip_draw_text(
                drm,
                stat_center[i] -
                    label_width / 2,
                190,
                stat_label[i],
                1,
                DIM);

            char value_visible[32];

            fit_text_to_width(
                stat_value[i],
                value_visible,
                sizeof(value_visible),
                150,
                UI_SCALE_BODY);

            int value_width =
                ui_text_width_px(
                    value_visible,
                    UI_SCALE_BODY);

            flip_draw_text(
                drm,
                stat_center[i] -
                    value_width / 2,
                211,
                value_visible,
                UI_SCALE_BODY,
                FG);
        }

        const char *cta =
            can_resume
                ? "A CONTINUE"
                : "A PLAY AGAIN";

        flip_draw_text(
            drm,
            centred_text_x_in_rect(
                cta,
                1,
                HOME_HERO_X,
                HOME_HERO_W),
            255,
            cta,
            1,
            selected == 0
                ? FG
                : DIM);

    } else {

        flip_draw_text(
            drm,
            centred_text_x_in_rect(
                "YOUR GAMES. YOUR TIME.",
                UI_SCALE_SECTION,
                HOME_HERO_X,
                HOME_HERO_W),
            116,
            "YOUR GAMES. YOUR TIME.",
            UI_SCALE_SECTION,
            FG);

        flip_draw_text(
            drm,
            centred_text_x_in_rect(
                "NO GAMES PLAYED YET",
                UI_SCALE_BODY,
                HOME_HERO_X,
                HOME_HERO_W),
            175,
            "NO GAMES PLAYED YET",
            UI_SCALE_BODY,
            DIM);

        flip_draw_text(
            drm,
            centred_text_x_in_rect(
                "A OPEN LIBRARY",
                1,
                HOME_HERO_X,
                HOME_HERO_W),
            250,
            "A OPEN LIBRARY",
            1,
            selected == 0
                ? FG
                : DIM);
    }

    flip_draw_text(
        drm,
        HOME_MARGIN_X,
        HOME_RECENT_LABEL_Y,
        "RECENT",
        1,
        DIM);

    int recent_cards = 0;

    for (int recent_index = 1;
         recent_index < recent_count &&
         recent_cards < 3;
         ++recent_index,
         ++recent_cards) {

        int card_x =
            HOME_MARGIN_X +
            recent_cards *
                (HOME_RECENT_CARD_W +
                 HOME_RECENT_GAP);

        flip_draw_rect(
            drm,
            card_x,
            HOME_RECENT_Y,
            HOME_RECENT_CARD_W,
            HOME_RECENT_H,
            PANEL);

        flip_draw_rect_outline(
            drm,
            card_x,
            HOME_RECENT_Y,
            HOME_RECENT_CARD_W,
            HOME_RECENT_H,
            1,
            DIM);

        int recent_scale =
            UI_SCALE_BODY;

        if (ui_text_width_px(
                recent_roms[recent_index].display,
                recent_scale) >
            HOME_RECENT_CARD_W - 24) {

            recent_scale = 1;
        }

        char recent_name[64];

        fit_text_to_width(
            recent_roms[recent_index].display,
            recent_name,
            sizeof(recent_name),
            HOME_RECENT_CARD_W - 24,
            recent_scale);

        flip_draw_text(
            drm,
            centred_text_x_in_rect(
                recent_name,
                recent_scale,
                card_x,
                HOME_RECENT_CARD_W),
            328,
            recent_name,
            recent_scale,
            FG);

        const char *recent_system =
            recent_roms[recent_index].system &&
            recent_roms[recent_index].system->display_name
                ? recent_roms[recent_index].system->display_name
                : "GAME";

        char recent_system_visible[32];

        fit_text_to_width(
            recent_system,
            recent_system_visible,
            sizeof(recent_system_visible),
            HOME_RECENT_CARD_W - 24,
            1);

        flip_draw_text(
            drm,
            centred_text_x_in_rect(
                recent_system_visible,
                1,
                card_x,
                HOME_RECENT_CARD_W),
            363,
            recent_system_visible,
            1,
            DIM);
    }

    if (recent_cards == 0) {
        flip_draw_text(
            drm,
            centred_text_x(
                recent_count > 0
                    ? "PLAY ANOTHER GAME TO BUILD RECENTS"
                    : "RECENT GAMES WILL APPEAR HERE",
                1),
            346,
            recent_count > 0
                ? "PLAY ANOTHER GAME TO BUILD RECENTS"
                : "RECENT GAMES WILL APPEAR HERE",
            1,
            DIM);
    }

    static const char *nav_label[4] = {
        "LIBRARY",
        "SWITCHER",
        "PLAY STATS",
        "SETTINGS"
    };

    for (int i = 0;
         i < 4;
         ++i) {

        int nav_index =
            i + 1;

        int nav_x =
            HOME_MARGIN_X +
            i *
                (HOME_NAV_W +
                 HOME_NAV_GAP);

        int active =
            selected == nav_index;

        if (active) {
            flip_draw_rect(
                drm,
                nav_x,
                HOME_NAV_Y,
                HOME_NAV_W,
                HOME_NAV_H,
                PANEL);

            flip_draw_rect_outline(
                drm,
                nav_x,
                HOME_NAV_Y,
                HOME_NAV_W,
                HOME_NAV_H,
                2,
                FG);
        }

        flip_draw_text(
            drm,
            centred_text_x_in_rect(
                nav_label[i],
                UI_SCALE_BODY,
                nav_x,
                HOME_NAV_W),
            HOME_NAV_Y + 9,
            nav_label[i],
            UI_SCALE_BODY,
            active
                ? FG
                : DIM);
    }

    if (message &&
        message[0]) {

        char visible_message[96];

        fit_text_to_width(
            message,
            visible_message,
            sizeof(visible_message),
            HOME_CONTENT_W,
            1);

        flip_draw_text(
            drm,
            centred_text_x(
                visible_message,
                1),
            HOME_MESSAGE_Y,
            visible_message,
            1,
            FG);
    }

    const char *footer =
        selected == 0
            ? "A PLAY   DOWN MORE"
            : "A OPEN   UP CONTINUE   LEFT RIGHT";

    flip_draw_text(
        drm,
        centred_text_x(
            footer,
            1),
        HOME_FOOTER_Y,
        footer,
        1,
        DIM);

    draw_battery_indicator(
        drm);

    flip_osd_set_poweroff_progress(
        poweroff_progress);

    flip_osd_draw(
        drm);

    flip_drm_present(
        drm);
}

static void draw_stats(
    flip_drm_t *drm,
    double poweroff_progress)
{
    flip_draw_clear(drm, BG);

    flip_draw_text(
        drm,
        centred_text_x(
            "PLAY STATS",
            UI_SCALE_TITLE),
        20,
        "PLAY STATS",
        UI_SCALE_TITLE,
        FG);

    flip_draw_text(
        drm,
        centred_text_x(
            "YOUR FLIPOS PLAY HISTORY",
            UI_SCALE_META),
        58,
        "YOUR FLIPOS PLAY HISTORY",
        UI_SCALE_META,
        DIM);

    if (play_stats_summary.games_tracked == 0) {
        flip_draw_text(
            drm,
            centred_text_x(
                "NO PLAY STATS YET",
                UI_SCALE_SECTION),
            185,
            "NO PLAY STATS YET",
            UI_SCALE_SECTION,
            FG);

        flip_draw_text(
            drm,
            centred_text_x(
                "LAUNCH A GAME AND FLIPOS WILL",
                UI_SCALE_BODY),
            230,
            "LAUNCH A GAME AND FLIPOS WILL",
            UI_SCALE_BODY,
            DIM);

        flip_draw_text(
            drm,
            centred_text_x(
                "START BUILDING YOUR HISTORY.",
                UI_SCALE_BODY),
            258,
            "START BUILDING YOUR HISTORY.",
            UI_SCALE_BODY,
            DIM);
    } else {
        char total_time[32];
        char longest[32];
        char average[32];

        format_play_duration(
            play_stats_summary.total_play_seconds,
            total_time,
            sizeof(total_time));

        format_play_duration(
            play_stats_summary.longest_session_seconds,
            longest,
            sizeof(longest));

        unsigned long long average_seconds =
            play_stats_summary.total_launches > 0
                ? play_stats_summary.total_play_seconds /
                    play_stats_summary.total_launches
                : 0;

        format_play_duration(
            average_seconds,
            average,
            sizeof(average));

        static const char *labels[3] = {
            "TOTAL PLAY TIME",
            "LAUNCHES",
            "GAMES PLAYED"
        };

        char values[3][32];
        snprintf(
            values[0],
            sizeof(values[0]),
            "%s",
            total_time);
        snprintf(
            values[1],
            sizeof(values[1]),
            "%llu",
            play_stats_summary.total_launches);
        snprintf(
            values[2],
            sizeof(values[2]),
            "%u",
            play_stats_summary.games_tracked);

        for (int i = 0;
             i < 3;
             ++i) {

            int x =
                28 + i * 198;

            flip_draw_rect(
                drm,
                x,
                83,
                188,
                72,
                PANEL);

            flip_draw_rect_outline(
                drm,
                x,
                83,
                188,
                72,
                1,
                DIM);

            flip_draw_text(
                drm,
                x + (188 -
                    (int)strlen(labels[i]) *
                    6 * UI_SCALE_META) / 2,
                96,
                labels[i],
                UI_SCALE_META,
                DIM);

            flip_draw_text(
                drm,
                x + (188 -
                    (int)strlen(values[i]) *
                    6 * UI_SCALE_SECTION) / 2,
                122,
                values[i],
                UI_SCALE_SECTION,
                FG);
        }

        char session_line[96];
        snprintf(
            session_line,
            sizeof(session_line),
            "LONGEST SESSION %s   AVERAGE %s",
            longest,
            average);

        flip_draw_text(
            drm,
            centred_text_x(
                session_line,
                UI_SCALE_META),
            174,
            session_line,
            UI_SCALE_META,
            DIM);

        flip_draw_text(
            drm,
            48,
            205,
            "MOST PLAYED",
            UI_SCALE_BODY,
            FG);

        for (size_t i = 0;
             i < play_stats_top_count &&
             i < PLAY_STATS_TOP_MAX;
             ++i) {

            int y =
                236 + (int)i * 43;

            char title[64];
            const char *base =
                strrchr(
                    play_stats_top[i].rom,
                    '/');

            base =
                base
                    ? base + 1
                    : play_stats_top[i].rom;

            const flip_system_t *system =
                flip_system_find(
                    play_stats_top[i].system);

            make_display_name(
                system,
                title,
                sizeof(title),
                base);

            char visible[25];
            snprintf(
                visible,
                sizeof(visible),
                "%.22s",
                title);

            char rank[8];
            snprintf(
                rank,
                sizeof(rank),
                "%u",
                (unsigned)i + 1U);

            flip_draw_text(
                drm,
                52,
                y,
                rank,
                UI_SCALE_BODY,
                DIM);

            flip_draw_text(
                drm,
                86,
                y,
                visible,
                UI_SCALE_BODY,
                FG);

            char played[32];
            format_play_duration(
                play_stats_top[i].total_play_seconds,
                played,
                sizeof(played));

            flip_draw_text(
                drm,
                472,
                y,
                played,
                UI_SCALE_BODY,
                FG);

            char launches[40];
            snprintf(
                launches,
                sizeof(launches),
                "%llu LAUNCH%s",
                play_stats_top[i].launches,
                play_stats_top[i].launches == 1
                    ? ""
                    : "ES");

            flip_draw_text(
                drm,
                87,
                y + 22,
                launches,
                1,
                DIM);
        }
    }

    flip_draw_text(
        drm,
        centred_text_x(
            "STATS ARE STORED LOCALLY ON YOUR SD CARD",
            1),
        420,
        "STATS ARE STORED LOCALLY ON YOUR SD CARD",
        1,
        DIM);

    flip_draw_text(
        drm,
        centred_text_x(
            "B BACK",
            UI_SCALE_HINT),
        452,
        "B BACK",
        UI_SCALE_HINT,
        DIM);

    draw_battery_indicator(drm);

    flip_osd_set_poweroff_progress(
        poweroff_progress);

    flip_osd_draw(drm);
    flip_drm_present(drm);
}


static void draw_game_details(
    flip_drm_t *drm,
    double poweroff_progress)
{
    flip_draw_clear(drm, BG);

    flip_draw_text(
        drm,
        centred_text_x(
            "GAME DETAILS",
            UI_SCALE_TITLE),
        18,
        "GAME DETAILS",
        UI_SCALE_TITLE,
        FG);

    char game_name[32];
    snprintf(
        game_name,
        sizeof(game_name),
        "%.29s",
        details_rom.display[0]
            ? details_rom.display
            : "GAME");

    flip_draw_text(
        drm,
        centred_text_x(
            game_name,
            UI_SCALE_SECTION),
        63,
        game_name,
        UI_SCALE_SECTION,
        FG);

    const char *system_name =
        details_rom.system &&
        details_rom.system->display_name
            ? details_rom.system->display_name
            : "GAME";

    int can_resume =
        details_rom.system &&
        flip_system_resume_available(
            details_rom.system,
            details_rom.path);

    char system_status[96];
    snprintf(
        system_status,
        sizeof(system_status),
        "%s%s",
        system_name,
        can_resume
            ? "   RESUME READY"
            : "");

    flip_draw_text(
        drm,
        centred_text_x(
            system_status,
            UI_SCALE_META),
        96,
        system_status,
        UI_SCALE_META,
        can_resume ? FG : DIM);

    flip_game_stats_t stats;
    int have_stats =
        details_rom.system &&
        details_rom.system->id &&
        flip_game_stats_load(
            details_rom.system->id,
            details_rom.path,
            &stats) == 1;

    if (!have_stats) {
        flip_draw_rect(
            drm,
            84,
            150,
            472,
            150,
            PANEL);

        flip_draw_rect_outline(
            drm,
            84,
            150,
            472,
            150,
            1,
            DIM);

        flip_draw_text(
            drm,
            centred_text_x(
                "NO PLAY STATS YET",
                UI_SCALE_SECTION),
            181,
            "NO PLAY STATS YET",
            UI_SCALE_SECTION,
            FG);

        flip_draw_text(
            drm,
            centred_text_x(
                "PLAY THIS GAME TO START",
                UI_SCALE_BODY),
            231,
            "PLAY THIS GAME TO START",
            UI_SCALE_BODY,
            DIM);

        flip_draw_text(
            drm,
            centred_text_x(
                "BUILDING ITS HISTORY.",
                UI_SCALE_BODY),
            260,
            "BUILDING ITS HISTORY.",
            UI_SCALE_BODY,
            DIM);
    } else {
        char played[32];
        char last_session[32];
        char longest[32];
        char average[32];
        char first_played[32];
        char last_played[32];

        format_play_duration(
            stats.total_play_seconds,
            played,
            sizeof(played));

        format_play_duration(
            stats.last_session_seconds,
            last_session,
            sizeof(last_session));

        format_play_duration(
            stats.longest_session_seconds,
            longest,
            sizeof(longest));

        unsigned long long average_seconds =
            stats.launches > 0
                ? stats.total_play_seconds /
                    stats.launches
                : 0;

        format_play_duration(
            average_seconds,
            average,
            sizeof(average));

        format_played_date(
            stats.first_played_epoch,
            first_played,
            sizeof(first_played));

        format_last_played(
            stats.last_played_epoch,
            last_played,
            sizeof(last_played));

        const int card_y = 125;
        const int card_w = 250;

        flip_draw_rect(
            drm,
            58,
            card_y,
            card_w,
            82,
            PANEL);
        flip_draw_rect_outline(
            drm,
            58,
            card_y,
            card_w,
            82,
            1,
            DIM);

        flip_draw_text(
            drm,
            78,
            card_y + 14,
            "TOTAL PLAY TIME",
            UI_SCALE_META,
            DIM);
        flip_draw_text(
            drm,
            78,
            card_y + 42,
            played,
            UI_SCALE_SECTION,
            FG);

        flip_draw_rect(
            drm,
            332,
            card_y,
            card_w,
            82,
            PANEL);
        flip_draw_rect_outline(
            drm,
            332,
            card_y,
            card_w,
            82,
            1,
            DIM);

        flip_draw_text(
            drm,
            352,
            card_y + 14,
            "LAUNCHES",
            UI_SCALE_META,
            DIM);

        char launches[32];
        snprintf(
            launches,
            sizeof(launches),
            "%llu",
            stats.launches);

        flip_draw_text(
            drm,
            352,
            card_y + 42,
            launches,
            UI_SCALE_SECTION,
            FG);

        static const char *labels[] = {
            "LAST SESSION",
            "LONGEST SESSION",
            "AVERAGE SESSION",
            "FIRST PLAYED",
            "LAST PLAYED"
        };

        const char *values[] = {
            last_session,
            longest,
            average,
            first_played,
            last_played
        };

        for (int i = 0;
             i < 5;
             ++i) {

            int y =
                229 + i * 34;

            flip_draw_text(
                drm,
                74,
                y,
                labels[i],
                UI_SCALE_META,
                DIM);

            int value_width =
                (int)strlen(values[i]) *
                6 *
                UI_SCALE_META;

            flip_draw_text(
                drm,
                566 - value_width,
                y,
                values[i],
                UI_SCALE_META,
                FG);
        }
    }

    flip_draw_text(
        drm,
        centred_text_x(
            "Y FROM LIBRARY OR GAMESWITCHER OPENS DETAILS",
            1),
        420,
        "Y FROM LIBRARY OR GAMESWITCHER OPENS DETAILS",
        1,
        DIM);

    flip_draw_text(
        drm,
        centred_text_x(
            "B BACK",
            UI_SCALE_HINT),
        452,
        "B BACK",
        UI_SCALE_HINT,
        DIM);

    draw_battery_indicator(drm);

    flip_osd_set_poweroff_progress(
        poweroff_progress);

    flip_osd_draw(drm);
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
        centred_text_x("SYSTEMS", UI_SCALE_TITLE),
        28,
        "SYSTEMS",
        UI_SCALE_TITLE,
        FG);

    char count_text[64];

    snprintf(
        count_text,
        sizeof(count_text),
        "%d SYSTEMS",
        available_system_count);

    flip_draw_text(
        drm,
        centred_text_x(count_text, UI_SCALE_META),
        72,
        count_text,
        UI_SCALE_META,
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
                "ADD GAMES TO THE SD CARD",
                1),
            225,
            "ADD GAMES TO THE SD CARD",
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
                125 + i * 58;

            if (i == selected) {

                flip_draw_rect(
                    drm,
                    115,
                    y - 10,
                    410,
                    44,
                    PANEL);

                flip_draw_rect_outline(
                    drm,
                    115,
                    y - 10,
                    410,
                    44,
                    2,
                    FG);
            }

            const char *name =
                system->display_name
                    ? system->display_name
                    : system->id;

            flip_draw_text(
                drm,
                centred_text_x(name, UI_SCALE_SECTION),
                y,
                name,
                UI_SCALE_SECTION,
                i == selected
                    ? FG
                    : DIM);
        }
    }

    flip_draw_text(
        drm,
        centred_text_x(
            "HOLD MENU + SELECT TO POWER OFF",
            1),
        420,
        "HOLD MENU + SELECT TO POWER OFF",
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

    draw_battery_indicator(
        drm);

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
        centred_text_x(count_text, UI_SCALE_META),
        61,
        count_text,
        UI_SCALE_META,
        DIM);

    if (rom_count <= 0) {

        char no_games[96];
        snprintf(
            no_games,
            sizeof(no_games),
            "NO %s GAMES FOUND",
            system_title);

        flip_draw_text(
            drm,
            centred_text_x(no_games, 2),
            190,
            no_games,
            2,
            FG);

        flip_draw_text(
            drm,
            centred_text_x("ADD GAMES TO THE SD CARD", UI_SCALE_BODY),
            225,
            "ADD GAMES TO THE SD CARD",
            UI_SCALE_BODY,
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
                92 + row * 44;

            if (index == selected) {

                flip_draw_rect(
                    drm,
                    45,
                    y - 7,
                    550,
                    36,
                    PANEL);

                flip_draw_rect_outline(
                    drm,
                    45,
                    y - 7,
                    550,
                    36,
                    2,
                    FG);
            }

            /*
             * Primary browser rows use the shared section scale. At scale 3,
             * 29 characters fit comfortably inside the 550px list card.
             */
            char visible[32];

            snprintf(
                visible,
                sizeof(visible),
                "%.29s",
                roms[index].display);

            flip_draw_text(
                drm,
                58,
                y,
                visible,
                UI_SCALE_SECTION,
                index == selected
                    ? FG
                    : DIM);

        }
    }

    flip_draw_text(
        drm,
        centred_text_x(
            "HOLD MENU + SELECT TO POWER OFF",
            1),
        420,
        "HOLD MENU + SELECT TO POWER OFF",
        1,
        DIM);

    flip_draw_text(
        drm,
        centred_text_x(
            "A LAUNCH   Y DETAILS   B BACK",
            1),
        452,
        "A LAUNCH   Y DETAILS   B BACK",
        1,
        DIM);

    draw_battery_indicator(
        drm);

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

    flip_draw_clear(
        drm,
        BG);

    load_suspended_session();

    flip_draw_text(
        drm,
        centred_text_x(
            "GAMESWITCHER",
            UI_SCALE_BODY),
        14,
        "GAMESWITCHER",
        UI_SCALE_BODY,
        FG);

    char count_text[32];

    snprintf(
        count_text,
        sizeof(count_text),
        "%d GAME%s",
        recent_count,
        recent_count == 1
            ? ""
            : "S");

    flip_draw_text(
        drm,
        centred_text_x(
            count_text,
            1),
        44,
        count_text,
        1,
        DIM);

    if (recent_count <= 0) {

        flip_draw_rect(
            drm,
            SWITCHER_HERO_X,
            SWITCHER_HERO_Y,
            SWITCHER_HERO_W,
            SWITCHER_HERO_H,
            PANEL);

        flip_draw_rect_outline(
            drm,
            SWITCHER_HERO_X,
            SWITCHER_HERO_Y,
            SWITCHER_HERO_W,
            SWITCHER_HERO_H,
            1,
            DIM);

        flip_draw_text(
            drm,
            centred_text_x_in_rect(
                "NO GAMES YET",
                UI_SCALE_SECTION,
                SWITCHER_HERO_X,
                SWITCHER_HERO_W),
            126,
            "NO GAMES YET",
            UI_SCALE_SECTION,
            FG);

        flip_draw_text(
            drm,
            centred_text_x_in_rect(
                "LAUNCH A GAME AND IT WILL APPEAR HERE",
                1,
                SWITCHER_HERO_X,
                SWITCHER_HERO_W),
            188,
            "LAUNCH A GAME AND IT WILL APPEAR HERE",
            1,
            DIM);

    } else {

        if (selected < 0)
            selected = 0;

        if (selected >= recent_count)
            selected = recent_count - 1;

        const rom_entry_t *current =
            &recent_roms[selected];

        char session_state[32];

        int have_session =
            recent_session_registry_state(
                current,
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

        int can_resume =
            flip_system_resume_available(
                current->system,
                current->path);

        const char *status =
            is_suspended
                ? "PAUSED"
                : needs_recovery
                    ? "RESTORE AVAILABLE"
                    : can_resume
                        ? "RESUME READY"
                        : "READY TO PLAY";

        flip_draw_rect(
            drm,
            SWITCHER_HERO_X,
            SWITCHER_HERO_Y,
            SWITCHER_HERO_W,
            SWITCHER_HERO_H,
            PANEL);

        flip_draw_rect_outline(
            drm,
            SWITCHER_HERO_X,
            SWITCHER_HERO_Y,
            SWITCHER_HERO_W,
            SWITCHER_HERO_H,
            2,
            FG);

        const char *system_name =
            current->system &&
            current->system->display_name
                ? current->system->display_name
                : "GAME";

        char system_visible[64];

        fit_text_to_width(
            system_name,
            system_visible,
            sizeof(system_visible),
            SWITCHER_HERO_W - 48,
            1);

        flip_draw_text(
            drm,
            centred_text_x_in_rect(
                system_visible,
                1,
                SWITCHER_HERO_X,
                SWITCHER_HERO_W),
            89,
            system_visible,
            1,
            DIM);

        int title_scale =
            UI_SCALE_SECTION;

        while (title_scale > 1 &&
               ui_text_width_px(
                   current->display,
                   title_scale) >
                   SWITCHER_HERO_W - 64) {

            title_scale--;
        }

        char title[96];

        fit_text_to_width(
            current->display,
            title,
            sizeof(title),
            SWITCHER_HERO_W - 64,
            title_scale);

        flip_draw_text(
            drm,
            centred_text_x_in_rect(
                title,
                title_scale,
                SWITCHER_HERO_X,
                SWITCHER_HERO_W),
            116,
            title,
            title_scale,
            FG);

        flip_draw_text(
            drm,
            centred_text_x_in_rect(
                status,
                1,
                SWITCHER_HERO_X,
                SWITCHER_HERO_W),
            154,
            status,
            1,
            is_suspended ||
            needs_recovery ||
            can_resume
                ? FG
                : DIM);

        flip_game_stats_t selected_stats;

        int have_stats =
            current->system &&
            current->system->id &&
            flip_game_stats_load(
                current->system->id,
                current->path,
                &selected_stats) == 1;

        char play_time[32] = "0M";
        char last_played[32] = "NOT YET";

        if (have_stats) {
            format_play_duration(
                selected_stats.total_play_seconds,
                play_time,
                sizeof(play_time));

            format_last_played(
                selected_stats.last_played_epoch,
                last_played,
                sizeof(last_played));
        }

        char stats_line[96];

        snprintf(
            stats_line,
            sizeof(stats_line),
            "%s   %llu LAUNCH%s   %s",
            play_time,
            have_stats
                ? selected_stats.launches
                : 0ULL,
            have_stats &&
            selected_stats.launches == 1
                ? ""
                : "ES",
            last_played);

        char stats_visible[96];

        fit_text_to_width(
            stats_line,
            stats_visible,
            sizeof(stats_visible),
            SWITCHER_HERO_W - 48,
            1);

        flip_draw_text(
            drm,
            centred_text_x_in_rect(
                stats_visible,
                1,
                SWITCHER_HERO_X,
                SWITCHER_HERO_W),
            187,
            stats_visible,
            1,
            DIM);

        const char *primary =
            can_resume
                ? "A RESUME"
                : "A LAUNCH";

        flip_draw_text(
            drm,
            centred_text_x_in_rect(
                primary,
                UI_SCALE_BODY,
                SWITCHER_HERO_X,
                SWITCHER_HERO_W),
            226,
            primary,
            UI_SCALE_BODY,
            FG);

        flip_draw_text(
            drm,
            SWITCHER_MARGIN_X,
            SWITCHER_STRIP_LABEL_Y,
            "RECENT SESSIONS",
            1,
            DIM);

        /*
         * Keep the selected item visible in a three-card strip. Cards are
         * fixed width, so titles must fit inside their own 168 px text area.
         */
        int first =
            selected - 1;

        if (first < 0)
            first = 0;

        if (first > recent_count - 3)
            first = recent_count - 3;

        if (first < 0)
            first = 0;

        for (int slot = 0;
             slot < 3;
             ++slot) {

            int index =
                first + slot;

            if (index >= recent_count)
                break;

            int card_x =
                SWITCHER_MARGIN_X +
                slot *
                    (SWITCHER_CARD_W +
                     SWITCHER_CARD_GAP);

            int active =
                index == selected;

            flip_draw_rect(
                drm,
                card_x,
                SWITCHER_CARD_Y,
                SWITCHER_CARD_W,
                SWITCHER_CARD_H,
                PANEL);

            flip_draw_rect_outline(
                drm,
                card_x,
                SWITCHER_CARD_Y,
                SWITCHER_CARD_W,
                SWITCHER_CARD_H,
                active ? 2 : 1,
                active
                    ? FG
                    : DIM);

            int card_title_scale =
                UI_SCALE_BODY;

            if (ui_text_width_px(
                    recent_roms[index].display,
                    card_title_scale) >
                SWITCHER_CARD_W - 24) {

                card_title_scale = 1;
            }

            char card_title[64];

            fit_text_to_width(
                recent_roms[index].display,
                card_title,
                sizeof(card_title),
                SWITCHER_CARD_W - 24,
                card_title_scale);

            flip_draw_text(
                drm,
                centred_text_x_in_rect(
                    card_title,
                    card_title_scale,
                    card_x,
                    SWITCHER_CARD_W),
                SWITCHER_CARD_Y + 18,
                card_title,
                card_title_scale,
                active
                    ? FG
                    : DIM);

            const char *card_system =
                recent_roms[index].system &&
                recent_roms[index].system->display_name
                    ? recent_roms[index].system->display_name
                    : "GAME";

            char card_system_visible[32];

            fit_text_to_width(
                card_system,
                card_system_visible,
                sizeof(card_system_visible),
                SWITCHER_CARD_W - 24,
                1);

            flip_draw_text(
                drm,
                centred_text_x_in_rect(
                    card_system_visible,
                    1,
                    card_x,
                    SWITCHER_CARD_W),
                SWITCHER_CARD_Y + 57,
                card_system_visible,
                1,
                DIM);

            if (flip_system_resume_available(
                    recent_roms[index].system,
                    recent_roms[index].path)) {

                flip_draw_text(
                    drm,
                    centred_text_x_in_rect(
                        "RESUME",
                        1,
                        card_x,
                        SWITCHER_CARD_W),
                    SWITCHER_CARD_Y + 76,
                    "RESUME",
                    1,
                    active
                        ? FG
                        : DIM);
            }
        }
    }

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
                "A RESUME   X END   Y DETAILS   B BACK";

        } else if (
            can_resume) {

            recent_help =
                "A RESUME   X REMOVE   Y DETAILS   B BACK";

        } else {

            recent_help =
                "A LAUNCH   X REMOVE   Y DETAILS   B BACK";
        }
    }

    char help_visible[96];

    fit_text_to_width(
        recent_help,
        help_visible,
        sizeof(help_visible),
        SWITCHER_CONTENT_W,
        1);

    flip_draw_text(
        drm,
        centred_text_x(
            help_visible,
            1),
        SWITCHER_FOOTER_Y,
        help_visible,
        1,
        DIM);

    if (gameswitcher_confirm !=
        GAMESWITCHER_CONFIRM_NONE) {

        const char *confirm_text =
            gameswitcher_confirm ==
                GAMESWITCHER_CONFIRM_CLOSE
                ? "END PAUSED GAME?"
                : "REMOVE FROM LIST?";

        flip_draw_rect(
            drm,
            115,
            395,
            410,
            50,
            PANEL);

        flip_draw_rect_outline(
            drm,
            115,
            395,
            410,
            50,
            1,
            FG);

        flip_draw_text(
            drm,
            centred_text_x(
                confirm_text,
                UI_SCALE_BODY),
            404,
            confirm_text,
            UI_SCALE_BODY,
            FG);

        flip_draw_text(
            drm,
            centred_text_x(
                "A CONFIRM   B CANCEL",
                1),
            430,
            "A CONFIRM   B CANCEL",
            1,
            DIM);
    }

    draw_battery_indicator(
        drm);

    flip_osd_set_poweroff_progress(
        poweroff_progress);

    flip_osd_draw(
        drm);

    flip_drm_present(
        drm);
}

static int launch_game_once(
    flip_drm_t *drm,
    const rom_entry_t *rom,
    int automatic_resume,
    unsigned long long *active_seconds,
    int *launched_any)
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

    int session_rc =
        flip_session_launch(
            drm,
            runner,
            argv,
            &metadata,
            &result);

    if (result.launched) {
        if (active_seconds)
            *active_seconds +=
                result.active_seconds;

        if (launched_any)
            *launched_any = 1;

        /*
         * A lid-open automatic continuation is the same user play session,
         * not another launch. A deliberate launch or GameSwitcher resume is
         * counted once when the emulator child genuinely starts.
         */
        if (!automatic_resume) {
            long long launch_epoch =
                (long long)time(NULL);

            if (launch_epoch > 0 &&
                result.active_seconds <=
                    (unsigned long long)launch_epoch) {
                launch_epoch -=
                    (long long)result.active_seconds;
            }

            if (flip_game_stats_record_launch(
                    system->id,
                    rom->path,
                    launch_epoch) < 0) {

                fprintf(
                    stderr,
                    "FRONTEND_STATS launch write failed rom=%s\n",
                    rom->path);
            }
        }
    }

    if (session_rc < 0) {

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
        "FRONTEND_GAME session_action=%d active_seconds=%llu\n",
        result.action,
        result.active_seconds);

    return (int)result.action;
}

static void finish_game_stats(
    const rom_entry_t *rom,
    unsigned long long active_seconds,
    int launched_any)
{
    if (!launched_any ||
        !rom ||
        !rom->system ||
        !rom->system->id)
        return;

    if (flip_game_stats_record_session(
            rom->system->id,
            rom->path,
            active_seconds,
            (long long)time(NULL)) < 0) {

        fprintf(
            stderr,
            "FRONTEND_STATS session write failed rom=%s\n",
            rom->path);
    }
}

static int launch_game(
    flip_drm_t *drm,
    const rom_entry_t *rom)
{
    if (!rom ||
        !rom->system ||
        !rom->system->id)
        return -1;

    int automatic_resume = 0;
    int launched_any = 0;
    unsigned long long active_seconds = 0;

    for (;;) {

        int result =
            launch_game_once(
                drm,
                rom,
                automatic_resume,
                &active_seconds,
                &launched_any);

        /* The intent applies to exactly one post-wake relaunch. */
        automatic_resume = 0;

        if (result != FLIP_SESSION_ACTION_SUSPEND) {
            finish_game_stats(
                rom,
                active_seconds,
                launched_any);
            return result;
        }

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

            finish_game_stats(
                rom,
                active_seconds,
                launched_any);
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

            finish_game_stats(
                rom,
                active_seconds,
                launched_any);
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

            finish_game_stats(
                rom,
                active_seconds,
                launched_any);
            return -1;
        }

        fprintf(
            stderr,
            "FRONTEND_STANDBY auto resume rom=%s\n",
            rom->path);

        /*
         * This next launch is a continuation of the lid-suspended session.
         * The session runtime reports only actual emulator child time, so the
         * sleep interval above never inflates play-time statistics.
         */
        automatic_resume = 1;
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
    screen_t dev_last_screen = (screen_t)-1;
    flip_keyboard_init(&wifi_keyboard);
    refresh_time_status();
    /*
     * Enforce the user's saved Automatic Time preference at frontend start.
     * The stock image starts ntpd during boot, so a manual-time user needs
     * FlipOS to stop it again before Wi-Fi can overwrite their chosen clock.
     */
    (void)flip_time_set_automatic(time_status.automatic);
    refresh_time_status();

    int home_selected = 0;
    int game_selected = 0;
    int recent_selected = 0;
    int settings_selected = 0;
    int connectivity_selected = 0;
    int system_settings_selected = 0;
    int interface_selected = 0;
    int optimizer_selected = 0;
    int optimizer_confirm = 0;

    int prev_up = 0;
    int prev_down = 0;
    int prev_a = 0;
    int prev_b = 0;
    int prev_x = 0;
    int prev_y = 0;
    int prev_left = 0;
    int prev_right = 0;
    int prev_start = 0;

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
    double next_battery_refresh =
        monotonic_seconds() + 30.0;

    flip_low_battery_guard_t low_battery_guard;
    flip_low_battery_guard_init(&low_battery_guard);
    double next_low_battery_check = monotonic_seconds();
    int low_battery_warning_visible = 0;

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
    refresh_play_stats();
    load_suspended_session();

    memset(&wifi_status, 0, sizeof(wifi_status));
    (void)flip_wifi_get_status(&wifi_status);

    int first_boot_required =
        !flip_setup_is_complete() ||
        access(FLIP_DEV_FORCE_FIRST_BOOT, F_OK) == 0;

    if (first_boot_required) {
        (void)flip_setup_begin();
        screen = SCREEN_FIRST_BOOT_WELCOME;
        draw_first_boot(&drm, screen, 0.0);
    } else if (access(
            OPTIMIZER_VALIDATION_RETURN_PATH,
            F_OK) == 0) {
        screen = SCREEN_OPTIMIZER_RESULTS;
        (void)unlink(OPTIMIZER_VALIDATION_RETURN_PATH);
        optimizer_message[0] = '\0';
        draw_optimizer_results(&drm, 0.0);
    } else if (access(
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

        if (screen != dev_last_screen) {
            dev_publish_screen_state(screen);
            dev_last_screen = screen;
        }

        int optimizer_validation_b_handled = 0;

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
                prev_y = 0;
                prev_left = 0;
                prev_right = 0;
                prev_start = 0;

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

                (void)flip_wifi_get_status(&wifi_status);

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
                        SCREEN_STATS) {

                    draw_stats(
                        &drm,
                        0.0);

                } else if (
                    screen ==
                        SCREEN_GAME_DETAILS) {

                    draw_game_details(
                        &drm,
                        0.0);

                } else if (
                    screen ==
                        SCREEN_SETTINGS) {

                    draw_settings(
                        &drm,
                        settings_selected,
                        0.0);

                } else if (screen == SCREEN_SETTINGS_CONNECTIVITY) {

                    draw_settings_menu(
                        &drm, "CONNECTIVITY", connectivity_items,
                        CONNECTIVITY_ITEM_COUNT, connectivity_selected, 0, 0.0);

                } else if (screen == SCREEN_SETTINGS_SYSTEM) {

                    draw_settings_menu(
                        &drm, "SYSTEM", system_settings_items,
                        SYSTEM_SETTINGS_ITEM_COUNT, system_settings_selected, 0, 0.0);

                } else if (screen == SCREEN_SETTINGS_TIME) {

                    draw_time_settings(&drm, 0.0);

                } else if (screen == SCREEN_SETTINGS_BIOS) {

                    draw_bios_verifier(&drm, 0.0);

                } else if (screen == SCREEN_SETTINGS_INTERFACE) {

                    draw_settings_menu(
                        &drm, "INTERFACE", interface_items,
                        INTERFACE_ITEM_COUNT, interface_selected, 1, 0.0);

                } else if (screen == SCREEN_WIFI_PASSWORD) {

                    draw_wifi_password(&drm, 0.0);

                } else if (screen == SCREEN_WIFI) {

                    (void)flip_wifi_get_status(&wifi_status);
                    draw_wifi(&drm, wifi_selected, 0.0);

                } else if (first_boot_screen(screen)) {

                    draw_first_boot(&drm, screen, 0.0);

                } else if (screen == SCREEN_UPDATE) {

                    draw_update(&drm, 0.0);

                } else if (screen == SCREEN_OPTIMIZER_RESULTS) {

                    draw_optimizer_results(&drm, 0.0);

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

        int y_button =
            state.down[
                FLIP_BTN_Y];

        int left =
            state.down[
                FLIP_BTN_LEFT];

        int right =
            state.down[
                FLIP_BTN_RIGHT];

        int start =
            state.down[
                FLIP_BTN_START];

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

        double power_now = monotonic_seconds();
        if (power_now >= next_low_battery_check) {
            flip_battery_snapshot_t battery;
            next_low_battery_check = power_now + 1.0;

            if (flip_battery_read(&battery) == 0) {
                flip_low_battery_action_t battery_action =
                    flip_low_battery_guard_update(
                        &low_battery_guard,
                        &battery,
                        1000U);

                if (battery_action == FLIP_LOW_BATTERY_WARN) {
                    if (!low_battery_warning_visible) {
                        flip_osd_show_status(
                            "LOW BATTERY - CONNECT CHARGER");
                        low_battery_warning_visible = 1;
                        redraw = 1;
                    }
                } else if (battery_action == FLIP_LOW_BATTERY_OK) {
                    low_battery_warning_visible = 0;
                }

                if (battery_action == FLIP_LOW_BATTERY_SHUTDOWN) {
                    flip_log_write(
                        FLIP_LOG_WARN,
                        "FLIP_POWER",
                        "frontend critical battery percent=%d voltage_uv=%ld external=%d",
                        battery.percent_valid ? battery.percent : -1,
                        battery.voltage_valid ? battery.voltage_uv : -1L,
                        battery.external_power_valid
                            ? battery.external_power_online
                            : -1);
                    power_off_low_battery(&drm);
                    break;
                }
            }
        }

        /* Keep the persistent battery indicator fresh on otherwise static screens. */
        double battery_now = monotonic_seconds();
        if (battery_now >= next_battery_refresh) {
            (void)flip_wifi_get_status(&wifi_status);
            if (!wifi_status.connected)
                wifi_topbar_signal_level = -1;
            redraw = 1;
            next_battery_refresh = battery_now + 30.0;
        }

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

                if (home_selected != 0)
                    home_selected = 0;

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

            } else if (screen == SCREEN_WIFI_PASSWORD) {

                flip_keyboard_move(&wifi_keyboard, 0, -1);

            } else if (
                screen ==
                    SCREEN_SETTINGS) {

                settings_selected--;

                if (settings_selected < 0)
                    settings_selected =
                        SETTINGS_ITEM_COUNT - 1;

            } else if (screen == SCREEN_SETTINGS_CONNECTIVITY) {

                connectivity_selected--;
                if (connectivity_selected < 0)
                    connectivity_selected = CONNECTIVITY_ITEM_COUNT - 1;

            } else if (screen == SCREEN_SETTINGS_SYSTEM) {

                system_settings_selected--;
                if (system_settings_selected < 0)
                    system_settings_selected = SYSTEM_SETTINGS_ITEM_COUNT - 1;

            } else if (screen == SCREEN_SETTINGS_TIME) {

                if (time_manual_editing)
                    adjust_manual_time_field(1);
                else {
                    time_selected--;
                    if (time_selected < 0)
                        time_selected = 3;
                }

            } else if (screen == SCREEN_SETTINGS_INTERFACE) {

                interface_selected--;
                if (interface_selected < 0)
                    interface_selected = INTERFACE_ITEM_COUNT - 1;

            } else if (
                screen == SCREEN_WIFI &&
                wifi_network_count > 0) {

                wifi_selected--;
                if (wifi_selected < 0)
                    wifi_selected = wifi_network_count - 1;

            } else if (
                screen ==
                    SCREEN_OPTIMIZER &&
                optimizer_confirm == 0) {

                if (optimizer_validation_selecting) {
                    optimizer_validation_duration_index--;
                    if (optimizer_validation_duration_index < 0)
                        optimizer_validation_duration_index =
                            OPTIMIZER_VALIDATION_DURATION_COUNT - 1;
                } else {
                    optimizer_selected--;
                    if (optimizer_selected < 0)
                        optimizer_selected =
                            OPTIMIZER_ACTION_COUNT - 1;
                }
            }

            redraw = 1;
        }

        if (down &&
            !prev_down) {

            if (screen == SCREEN_HOME) {

                if (home_selected == 0)
                    home_selected = 1;

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

            } else if (screen == SCREEN_WIFI_PASSWORD) {

                flip_keyboard_move(&wifi_keyboard, 0, 1);

            } else if (
                screen ==
                    SCREEN_SETTINGS) {

                settings_selected++;

                if (settings_selected >=
                    SETTINGS_ITEM_COUNT)
                    settings_selected = 0;

            } else if (screen == SCREEN_SETTINGS_CONNECTIVITY) {

                connectivity_selected++;
                if (connectivity_selected >= CONNECTIVITY_ITEM_COUNT)
                    connectivity_selected = 0;

            } else if (screen == SCREEN_SETTINGS_SYSTEM) {

                system_settings_selected++;
                if (system_settings_selected >= SYSTEM_SETTINGS_ITEM_COUNT)
                    system_settings_selected = 0;

            } else if (screen == SCREEN_SETTINGS_TIME) {

                if (time_manual_editing)
                    adjust_manual_time_field(-1);
                else {
                    time_selected++;
                    if (time_selected >= 4)
                        time_selected = 0;
                }

            } else if (screen == SCREEN_SETTINGS_INTERFACE) {

                interface_selected++;
                if (interface_selected >= INTERFACE_ITEM_COUNT)
                    interface_selected = 0;

            } else if (
                screen == SCREEN_WIFI &&
                wifi_network_count > 0) {

                wifi_selected++;
                if (wifi_selected >= wifi_network_count)
                    wifi_selected = 0;

            } else if (
                screen ==
                    SCREEN_OPTIMIZER &&
                optimizer_confirm == 0) {

                if (optimizer_validation_selecting) {
                    optimizer_validation_duration_index++;
                    if (optimizer_validation_duration_index >=
                        OPTIMIZER_VALIDATION_DURATION_COUNT)
                        optimizer_validation_duration_index = 0;
                } else {
                    optimizer_selected++;
                    if (optimizer_selected >=
                        OPTIMIZER_ACTION_COUNT)
                        optimizer_selected = 0;
                }
            }

            redraw = 1;
        }

        if (screen == SCREEN_WIFI_PASSWORD &&
            y_button && !prev_y) {
            flip_keyboard_toggle_case(&wifi_keyboard);
            redraw = 1;
        }

        if (y_button &&
            !prev_y &&
            gameswitcher_confirm ==
                GAMESWITCHER_CONFIRM_NONE &&
            screen == SCREEN_GAMES &&
            rom_count > 0) {

            details_rom =
                roms[game_selected];

            details_return_screen =
                SCREEN_GAMES;

            screen =
                SCREEN_GAME_DETAILS;

            redraw = 1;
        }

        if (y_button &&
            !prev_y &&
            gameswitcher_confirm ==
                GAMESWITCHER_CONFIRM_NONE &&
            screen == SCREEN_RECENT &&
            recent_count > 0) {

            details_rom =
                recent_roms[recent_selected];

            details_return_screen =
                SCREEN_RECENT;

            screen =
                SCREEN_GAME_DETAILS;

            redraw = 1;
        }

        if (screen == SCREEN_WIFI_PASSWORD &&
            x && !prev_x) {
            flip_keyboard_toggle_reveal(&wifi_keyboard);
            redraw = 1;
        }

        if (screen == SCREEN_RECENT &&
            recent_count > 0 &&
            left && !prev_left) {

            recent_selected--;

            if (recent_selected < 0)
                recent_selected =
                    recent_count - 1;

            redraw = 1;
        }

        if (screen == SCREEN_HOME &&
            left && !prev_left) {

            if (home_selected == 0) {
                home_selected =
                    HOME_ITEM_COUNT - 1;
            } else {
                home_selected--;

                if (home_selected < 1)
                    home_selected =
                        HOME_ITEM_COUNT - 1;
            }

            redraw = 1;
        }

        if (screen == SCREEN_RECENT &&
            recent_count > 0 &&
            right && !prev_right) {

            recent_selected++;

            if (recent_selected >=
                recent_count)
                recent_selected = 0;

            redraw = 1;
        }

        if (screen == SCREEN_HOME &&
            right && !prev_right) {

            if (home_selected == 0) {
                home_selected = 1;
            } else {
                home_selected++;

                if (home_selected >=
                    HOME_ITEM_COUNT)
                    home_selected = 1;
            }

            redraw = 1;
        }

        if ((screen == SCREEN_SETTINGS_TIME ||
             screen == SCREEN_FIRST_BOOT_TIME) &&
            left && !prev_left) {
            if (screen == SCREEN_SETTINGS_TIME && time_manual_editing) {
                time_manual_field--;
                if (time_manual_field < 0)
                    time_manual_field = 4;
            } else if (screen == SCREEN_FIRST_BOOT_TIME ||
                       time_selected == 1) {
                change_time_zone(-1);
            }
            redraw = 1;
        }

        if ((screen == SCREEN_SETTINGS_TIME ||
             screen == SCREEN_FIRST_BOOT_TIME) &&
            right && !prev_right) {
            if (screen == SCREEN_SETTINGS_TIME && time_manual_editing) {
                time_manual_field++;
                if (time_manual_field > 4)
                    time_manual_field = 0;
            } else if (screen == SCREEN_FIRST_BOOT_TIME ||
                       time_selected == 1) {
                change_time_zone(1);
            }
            redraw = 1;
        }

        if (screen == SCREEN_WIFI_PASSWORD &&
            left && !prev_left) {
            flip_keyboard_move(&wifi_keyboard, -1, 0);
            redraw = 1;
        }

        if (screen == SCREEN_WIFI_PASSWORD &&
            right && !prev_right) {
            flip_keyboard_move(&wifi_keyboard, 1, 0);
            redraw = 1;
        }

        int wifi_password_b_handled = 0;
        if (screen == SCREEN_WIFI_PASSWORD &&
            b && !prev_b) {
            size_t length = strlen(wifi_password);
            if (length > 0) {
                (void)flip_keyboard_backspace(wifi_password);
                wifi_password_message[0] = '\0';
            } else {
                screen = SCREEN_WIFI;
                clear_wifi_password();
                wifi_password_message[0] = '\0';
                flip_keyboard_init(&wifi_keyboard);
            }
            wifi_password_b_handled = 1;
            redraw = 1;
        }

        if (screen == SCREEN_FIRST_BOOT_WIFI &&
            start && !prev_start) {
            screen = SCREEN_FIRST_BOOT_TIME;
            redraw = 1;
        }

        /*
         * During first-boot onboarding, START must always provide an escape
         * from the Wi-Fi browser. A user who opened the scanner should not
         * have to discover that B returns to a separate setup page before
         * they can continue offline.
         */
        if (screen == SCREEN_WIFI &&
            wifi_return_screen == SCREEN_FIRST_BOOT_WIFI &&
            start && !prev_start) {
            screen = SCREEN_FIRST_BOOT_TIME;
            wifi_message[0] = '\0';
            redraw = 1;
        }

        if (screen == SCREEN_WIFI_PASSWORD &&
            start && !prev_start) {
            size_t length = strlen(wifi_password);
            if (length < 8) {
                snprintf(wifi_password_message,
                    sizeof(wifi_password_message),
                    "PASSWORD MUST BE AT LEAST 8 CHARACTERS");
                redraw = 1;
            } else if (wifi_network_count > 0 &&
                       wifi_selected >= 0 &&
                       wifi_selected < wifi_network_count) {
                snprintf(wifi_password_message,
                    sizeof(wifi_password_message),
                    "CONNECTING...");
                draw_wifi_password(&drm, 0.0);
                int connect_result = flip_wifi_connect(
                    wifi_networks[wifi_selected].ssid,
                    wifi_password);
                clear_wifi_password();
                flip_keyboard_init(&wifi_keyboard);
                (void)flip_wifi_get_status(&wifi_status);
                screen = SCREEN_WIFI;
                if (connect_result == FLIP_WIFI_CONNECT_OK &&
                    wifi_status.connected && wifi_status.usable_ip) {
                    snprintf(wifi_message, sizeof(wifi_message),
                        "CONNECTED");
                    if (wifi_return_screen == SCREEN_FIRST_BOOT_WIFI)
                        screen = SCREEN_FIRST_BOOT_TIME;
                } else if (connect_result == FLIP_WIFI_CONNECT_AUTH_NO_IP &&
                           wifi_status.connected) {
                    snprintf(wifi_message, sizeof(wifi_message),
                        "CONNECTED - NO NETWORK ADDRESS");
                } else {
                    snprintf(wifi_message, sizeof(wifi_message),
                        "CONNECTION FAILED");
                }
                wifi_password_message[0] = '\0';
                redraw = 1;
            }
        }

        if (b &&
            !prev_b &&
            screen == SCREEN_OPTIMIZER &&
            optimizer_validation_selecting) {

            optimizer_validation_selecting = 0;
            optimizer_validation_b_handled = 1;
            optimizer_message[0] = '\0';
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
            optimizer_confirm == 0 &&
            !optimizer_validation_selecting &&
            !optimizer_validation_b_handled &&
            !wifi_password_b_handled) {

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
                    SCREEN_RECENT ||
                screen ==
                    SCREEN_STATS) {

                screen =
                    SCREEN_HOME;

                message[0] =
                    '\0';

                redraw = 1;

            } else if (screen == SCREEN_WIFI) {

                screen = wifi_return_screen;
                wifi_message[0] = '\0';
                redraw = 1;

            } else if (screen == SCREEN_FIRST_BOOT_WIFI) {

                screen = SCREEN_FIRST_BOOT_WELCOME;
                redraw = 1;

            } else if (screen == SCREEN_FIRST_BOOT_TIME) {

                screen = SCREEN_FIRST_BOOT_WIFI;
                redraw = 1;

            } else if (screen == SCREEN_FIRST_BOOT_PREFS) {

                screen = SCREEN_FIRST_BOOT_TIME;
                redraw = 1;

            } else if (screen == SCREEN_FIRST_BOOT_BIOS) {

                screen = SCREEN_FIRST_BOOT_PREFS;
                redraw = 1;

            } else if (screen == SCREEN_FIRST_BOOT_READY) {

                screen = SCREEN_FIRST_BOOT_BIOS;
                redraw = 1;

            } else if (screen == SCREEN_GAME_DETAILS) {

                screen =
                    details_return_screen;

                redraw = 1;

            } else if (screen == SCREEN_UPDATE) {

                if (update_confirm) {
                    update_confirm = 0;
                    update_message[0] = '\0';
                } else {
                    screen = SCREEN_SETTINGS_SYSTEM;
                    update_message[0] = '\0';
                    update_complete = 0;
                }
                redraw = 1;

            } else if (
                screen ==
                    SCREEN_SETTINGS) {

                screen =
                    SCREEN_HOME;

                message[0] =
                    '\0';

                redraw = 1;

            } else if (screen == SCREEN_SETTINGS_TIME) {

                if (time_manual_editing) {
                    time_manual_editing = 0;
                    time_message[0] = '\0';
                } else {
                    screen = SCREEN_SETTINGS_SYSTEM;
                    time_message[0] = '\0';
                }
                redraw = 1;

            } else if (screen == SCREEN_SETTINGS_BIOS) {

                screen = SCREEN_SETTINGS_SYSTEM;
                redraw = 1;

            } else if (screen == SCREEN_SETTINGS_CONNECTIVITY ||
                       screen == SCREEN_SETTINGS_SYSTEM ||
                       screen == SCREEN_SETTINGS_INTERFACE) {

                screen = SCREEN_SETTINGS;
                redraw = 1;

            } else if (
                screen == SCREEN_OPTIMIZER_RESULTS) {

                screen = SCREEN_OPTIMIZER;
                optimizer_message[0] = '\0';
                redraw = 1;

            } else if (
                screen ==
                    SCREEN_OPTIMIZER) {

                screen =
                    SCREEN_SETTINGS_SYSTEM;

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

            } else if (action == 3) {
                unsigned duration_minutes =
                    optimizer_validation_minutes[
                        optimizer_validation_duration_index];
                int start_rc =
                    optimizer_start_validation(duration_minutes);

                if (start_rc == 1) {
                    snprintf(optimizer_message, sizeof(optimizer_message),
                        "CONNECT CHARGER - BATTERY 30%%+");
                    redraw = 1;
                } else if (start_rc == 2) {
                    snprintf(optimizer_message, sizeof(optimizer_message),
                        "NO OPTIMIZED PROFILE");
                    redraw = 1;
                } else if (start_rc == 3) {
                    snprintf(optimizer_message, sizeof(optimizer_message),
                        "PROFILE QUARANTINED - REOPTIMIZE");
                    redraw = 1;
                } else if (start_rc < 0) {
                    snprintf(optimizer_message, sizeof(optimizer_message),
                        "STABILITY TEST START FAILED");
                    redraw = 1;
                } else {
                    snprintf(
                        optimizer_message,
                        sizeof(optimizer_message),
                        "STARTING %u MIN STABILITY TEST...",
                        duration_minutes);
                    draw_optimizer(&drm, optimizer_selected, 0,
                        optimizer_message, 0.0);
                    fprintf(stderr,
                        "FRONTEND_OPTIMIZER validation requested charger=1\n");
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

        if (screen == SCREEN_UPDATE &&
            y_button && !prev_y &&
            !update_confirm &&
            !update_complete) {
            char version[96];
            if (read_update_version(version, sizeof(version)) == 0) {
                if (delete_update_package() == 0) {
                    snprintf(
                        update_message,
                        sizeof(update_message),
                        "DOWNLOADED UPDATE DELETED");
                } else {
                    snprintf(
                        update_message,
                        sizeof(update_message),
                        "COULD NOT DELETE UPDATE");
                }
                redraw = 1;
            }
        }

        if (screen == SCREEN_WIFI &&
            y_button && !prev_y) {
            if (flip_wifi_forget() == 0) {
                memset(&wifi_status, 0, sizeof(wifi_status));
                snprintf(wifi_message, sizeof(wifi_message),
                    "SAVED NETWORK FORGOTTEN");
                wifi_selected = 0;
            } else {
                snprintf(wifi_message, sizeof(wifi_message),
                    "FORGET NETWORK FAILED");
            }
            redraw = 1;
        }

        if (screen == SCREEN_FIRST_BOOT_WIFI &&
            x && !prev_x) {
            screen = SCREEN_FIRST_BOOT_TIME;
            redraw = 1;
        }

        if (screen == SCREEN_FIRST_BOOT_TIME &&
            x && !prev_x) {
            (void)flip_wifi_get_status(&wifi_status);
            if (!wifi_status.connected || !wifi_status.usable_ip) {
                snprintf(
                    time_message,
                    sizeof(time_message),
                    "CONNECT WI-FI TO SYNC TIME");
            } else {
                snprintf(
                    time_message,
                    sizeof(time_message),
                    "SYNCING TIME...");
                draw_first_boot(&drm, screen, 0.0);
                if (flip_time_sync_ntp() == 0) {
                    refresh_time_status();
                    snprintf(
                        time_message,
                        sizeof(time_message),
                        "TIME SYNCED");
                } else {
                    snprintf(
                        time_message,
                        sizeof(time_message),
                        "TIME SYNC FAILED");
                }
            }
            redraw = 1;
        }

        if (screen == SCREEN_UPDATE &&
            x && !prev_x && !update_confirm) {
            snprintf(update_message, sizeof(update_message),
                "CHECKING GITHUB...");
            draw_update(&drm, 0.0);
            if (run_update_fetcher() == 0) {
                if (access(FLIP_UPDATE_AVAILABLE, F_OK) == 0) {
                    snprintf(update_message, sizeof(update_message),
                        "UPDATE DOWNLOADED - READY TO INSTALL");
                } else {
                    snprintf(update_message, sizeof(update_message),
                        "YOU'RE ON THE LATEST RELEASE");
                }
            } else {
                snprintf(update_message, sizeof(update_message),
                    "GITHUB CHECK FAILED - TRY AGAIN LATER");
            }
            redraw = 1;
        }

        if (screen == SCREEN_WIFI &&
            x && !prev_x) {
            snprintf(wifi_message, sizeof(wifi_message), "SCANNING...");
            draw_wifi(&drm, wifi_selected, 0.0);
            (void)refresh_wifi_networks();
            if (wifi_selected >= wifi_network_count)
                wifi_selected = wifi_network_count > 0
                    ? wifi_network_count - 1
                    : 0;
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
                refresh_play_stats();

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

            if (screen == SCREEN_FIRST_BOOT_WELCOME) {

                screen = SCREEN_FIRST_BOOT_WIFI;
                redraw = 1;

            } else if (screen == SCREEN_FIRST_BOOT_WIFI) {

                wifi_message[0] = '\0';
                wifi_selected = 0;
                wifi_return_screen = SCREEN_FIRST_BOOT_WIFI;
                screen = SCREEN_WIFI;
                draw_wifi(&drm, wifi_selected, 0.0);
                flip_osd_show_status("SCANNING WI-FI...");
                flip_osd_draw(&drm);
                flip_drm_present(&drm);
                (void)refresh_wifi_networks();
                redraw = 1;

            } else if (screen == SCREEN_FIRST_BOOT_TIME) {

                /*
                 * Automatic ntpd is already running in the background.
                 * Do not turn A/CONTINUE into a blocking one-shot NTP sync:
                 * the stock ntpd can legitimately take tens of seconds while
                 * Wi-Fi/DHCP settles, which made first boot look frozen.
                 * X remains the explicit "sync now" action.
                 */
                screen = SCREEN_FIRST_BOOT_PREFS;
                redraw = 1;

            } else if (screen == SCREEN_FIRST_BOOT_PREFS) {

                screen = SCREEN_FIRST_BOOT_BIOS;
                redraw = 1;

            } else if (screen == SCREEN_FIRST_BOOT_BIOS) {

                screen = SCREEN_FIRST_BOOT_READY;
                redraw = 1;

            } else if (screen == SCREEN_FIRST_BOOT_READY) {

                if (flip_setup_mark_complete() == 0) {
                    (void)unlink(FLIP_DEV_FORCE_FIRST_BOOT);
                    screen = SCREEN_HOME;
                    message[0] = '\0';
                    redraw = 1;
                } else {
                    flip_osd_show_status("SETUP COULD NOT BE SAVED");
                    redraw = 1;
                }

            } else if (screen ==
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

                    if (recent_count <= 0) {
                        refresh_available_systems();

                        if (system_selected >=
                            available_system_count) {

                            system_selected = 0;
                        }

                        screen =
                            SCREEN_SYSTEMS;

                        redraw = 1;

                    } else {

                        flip_draw_clear(
                            &drm,
                            BG);

                        int can_resume =
                            flip_system_resume_available(
                                recent_roms[0].system,
                                recent_roms[0].path);

                        const char *launch_text =
                            can_resume
                                ? "RESUMING"
                                : "LAUNCHING";

                        flip_draw_text(
                            &drm,
                            centred_text_x(
                                launch_text,
                                UI_SCALE_SECTION),
                            185,
                            launch_text,
                            UI_SCALE_SECTION,
                            FG);

                        char launch_name[64];
                        fit_text_to_width(
                            recent_roms[0].display,
                            launch_name,
                            sizeof(launch_name),
                            560,
                            UI_SCALE_BODY);

                        flip_draw_text(
                            &drm,
                            centred_text_x(
                                launch_name,
                                UI_SCALE_BODY),
                            230,
                            launch_name,
                            UI_SCALE_BODY,
                            DIM);

                        flip_drm_present(
                            &drm);

                        int home_game_result =
                            launch_game(
                                &drm,
                                &recent_roms[0]);

                        if (home_game_result < 0) {
                            fprintf(
                                stderr,
                                "FRONTEND_HOME lifecycle failed\n");
                            return 3;
                        }

                        if (home_game_result ==
                            FLIP_SESSION_ACTION_LOW_BATTERY) {

                            power_off_low_battery(
                                &drm);
                            break;
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
                        prev_x = 0;
                        prev_y = 0;
                        prev_left = 0;
                        prev_right = 0;
                        prev_start = 0;

                        poweroff_active = 0;

                        load_recent_history();
                        refresh_play_stats();

                        if (home_game_result ==
                            FLIP_SESSION_ACTION_SUSPEND) {

                            recent_selected = 0;

                            select_suspended_recent(
                                &recent_selected);

                            screen =
                                SCREEN_RECENT;

                            fprintf(
                                stderr,
                                "FRONTEND_GAMESWITCHER entered_from_home\n");

                            draw_recent(
                                &drm,
                                game_system,
                                recent_selected,
                                0.0);

                            continue;
                        }

                        screen =
                            SCREEN_HOME;

                        home_selected = 0;
                        message[0] = '\0';

                        draw_home(
                            &drm,
                            home_selected,
                            message,
                            0.0);

                        continue;
                    }

                } else if (
                    home_selected == 1) {

                    refresh_available_systems();

                    if (system_selected >=
                        available_system_count) {

                        system_selected = 0;
                    }

                    screen =
                        SCREEN_SYSTEMS;

                    redraw = 1;

                } else if (
                    home_selected == 2) {

                    load_recent_history();
                    refresh_play_stats();

                    screen =
                        SCREEN_RECENT;

                    redraw = 1;

                } else if (
                    home_selected == 3) {

                    refresh_play_stats();

                    screen =
                        SCREEN_STATS;

                    redraw = 1;

                } else if (
                    home_selected == 4) {

                    screen =
                        SCREEN_SETTINGS;

                    settings_selected = 0;
                    message[0] = '\0';

                    fprintf(
                        stderr,
                        "FRONTEND_SETTINGS entered\n");

                    redraw = 1;
                }

            } else if (
                screen ==
                    SCREEN_SETTINGS) {

                if (settings_selected == 0) {
                    connectivity_selected = 0;
                    screen = SCREEN_SETTINGS_CONNECTIVITY;
                } else if (settings_selected == 1) {
                    system_settings_selected = 0;
                    screen = SCREEN_SETTINGS_SYSTEM;
                } else {
                    interface_selected = 0;
                    screen = SCREEN_SETTINGS_INTERFACE;
                }
                redraw = 1;

            } else if (screen == SCREEN_SETTINGS_CONNECTIVITY) {

                wifi_message[0] = '\0';
                wifi_selected = 0;
                wifi_return_screen = SCREEN_SETTINGS_CONNECTIVITY;
                screen = SCREEN_WIFI;
                draw_wifi(&drm, wifi_selected, 0.0);
                flip_osd_show_status("SCANNING WI-FI...");
                flip_osd_draw(&drm);
                flip_drm_present(&drm);
                (void)refresh_wifi_networks();
                redraw = 1;

            } else if (screen == SCREEN_SETTINGS_SYSTEM) {

                if (system_settings_selected == 0) {
                    screen = SCREEN_OPTIMIZER;
                    optimizer_selected = 0;
                    optimizer_confirm = 0;
                    optimizer_message[0] = '\0';
                    fprintf(stderr, "FRONTEND_OPTIMIZER entered\n");
                } else if (system_settings_selected == 1) {
                    screen = SCREEN_UPDATE;
                    update_message[0] = '\0';
                    update_complete = 0;
                    update_confirm = 0;
                } else if (system_settings_selected == 2) {
                    screen = SCREEN_SETTINGS_TIME;
                    time_selected = 0;
                    time_manual_editing = 0;
                    time_message[0] = '\0';
                    refresh_time_status();
                    fprintf(stderr, "FRONTEND_TIME_SETTINGS entered\n");
                } else {
                    screen = SCREEN_SETTINGS_BIOS;
                    fprintf(stderr, "FRONTEND_BIOS_VERIFIER entered\n");
                }
                redraw = 1;

            } else if (screen == SCREEN_SETTINGS_TIME) {

                if (time_manual_editing) {
                    if (flip_time_set_manual(&time_manual_value) == 0) {
                        time_manual_editing = 0;
                        refresh_time_status();
                        snprintf(
                            time_message,
                            sizeof(time_message),
                            "DATE & TIME SAVED");
                    } else {
                        snprintf(
                            time_message,
                            sizeof(time_message),
                            "DATE & TIME COULD NOT BE SAVED");
                    }
                } else if (time_selected == 0) {
                    int enabled = !time_status.automatic;
                    if (flip_time_set_automatic(enabled) == 0) {
                        refresh_time_status();
                        snprintf(
                            time_message,
                            sizeof(time_message),
                            enabled
                                ? "AUTOMATIC TIME ON"
                                : "AUTOMATIC TIME OFF");
                    }
                } else if (time_selected == 1) {
                    change_time_zone(1);
                } else if (time_selected == 2) {
                    (void)flip_wifi_get_status(&wifi_status);
                    if (!wifi_status.connected || !wifi_status.usable_ip) {
                        snprintf(
                            time_message,
                            sizeof(time_message),
                            "CONNECT WI-FI TO SYNC TIME");
                    } else {
                        snprintf(
                            time_message,
                            sizeof(time_message),
                            "SYNCING TIME...");
                        draw_time_settings(&drm, 0.0);
                        if (flip_time_sync_ntp() == 0) {
                            refresh_time_status();
                            snprintf(
                                time_message,
                                sizeof(time_message),
                                "TIME SYNCED");
                        } else {
                            snprintf(
                                time_message,
                                sizeof(time_message),
                                "TIME SYNC FAILED");
                        }
                    }
                } else if (!time_status.automatic) {
                    begin_manual_time_edit();
                } else {
                    snprintf(
                        time_message,
                        sizeof(time_message),
                        "TURN AUTOMATIC TIME OFF FIRST");
                }
                redraw = 1;

            } else if (screen == SCREEN_SETTINGS_BIOS) {

                fprintf(stderr, "FRONTEND_BIOS_VERIFIER recheck\n");
                redraw = 1;

            } else if (screen == SCREEN_SETTINGS_INTERFACE) {

                int visible =
                    flip_settings_get_battery_percent_visible();
                (void)flip_settings_set_battery_percent_visible(!visible);
                (void)flip_settings_flush();
                redraw = 1;

            } else if (screen == SCREEN_WIFI) {
                if (wifi_network_count > 0) {
                    const flip_wifi_network_t *network =
                        &wifi_networks[wifi_selected];
                    if (network->secure) {
                        clear_wifi_password();
                        wifi_password_message[0] = '\0';
                        flip_keyboard_init(&wifi_keyboard);
                        screen = SCREEN_WIFI_PASSWORD;
                    } else {
                        snprintf(wifi_message, sizeof(wifi_message),
                            "CONNECTING...");
                        draw_wifi(&drm, wifi_selected, 0.0);
                        int connect_result =
                            flip_wifi_connect(network->ssid, NULL);
                        if (connect_result == FLIP_WIFI_CONNECT_OK) {
                            snprintf(wifi_message, sizeof(wifi_message),
                                "CONNECTED");
                            if (wifi_return_screen == SCREEN_FIRST_BOOT_WIFI)
                                screen = SCREEN_FIRST_BOOT_TIME;
                        } else if (connect_result == FLIP_WIFI_CONNECT_AUTH_NO_IP)
                            snprintf(wifi_message, sizeof(wifi_message),
                                "CONNECTED - NO NETWORK ADDRESS");
                        else
                            snprintf(wifi_message, sizeof(wifi_message),
                                "CONNECTION FAILED");
                        (void)flip_wifi_get_status(&wifi_status);
                    }
                    redraw = 1;
                }

            } else if (screen == SCREEN_WIFI_PASSWORD) {
                if (flip_keyboard_append_selected(
                        &wifi_keyboard,
                        wifi_password,
                        sizeof(wifi_password)) > 0) {
                    wifi_password_message[0] = '\0';
                }
                redraw = 1;

            } else if (screen == SCREEN_UPDATE) {
                if (update_complete) {
                    snprintf(
                        update_message,
                        sizeof(update_message),
                        "RESTARTING...");
                    draw_update(&drm, 0.0);
                    fflush(stderr);
                    sync();
                    reboot(RB_AUTOBOOT);
                    redraw = 1;
                } else {
                    char version[96];
                    if (read_update_version(version, sizeof(version)) < 0) {
                        update_confirm = 0;
                        snprintf(update_message, sizeof(update_message),
                            "CHECKING GITHUB...");
                        draw_update(&drm, 0.0);
                        if (run_update_fetcher() == 0) {
                            if (access(FLIP_UPDATE_AVAILABLE, F_OK) == 0) {
                                snprintf(update_message, sizeof(update_message),
                                    "UPDATE DOWNLOADED - READY TO INSTALL");
                            } else {
                                snprintf(update_message, sizeof(update_message),
                                    "YOU'RE ON THE LATEST RELEASE");
                            }
                        } else {
                            snprintf(update_message, sizeof(update_message),
                                "GITHUB CHECK FAILED - TRY AGAIN LATER");
                        }
                        redraw = 1;
                    } else if (!update_confirm) {
                        update_confirm = 1;
                        update_message[0] = '\0';
                        redraw = 1;
                    } else {
                        update_confirm = 0;
                        snprintf(update_message, sizeof(update_message),
                            "VERIFYING AND INSTALLING...");
                        draw_update(&drm, 0.0);
                        if (run_update_installer() == 0) {
                            update_complete = 1;
                            snprintf(update_message, sizeof(update_message),
                                "UPDATE VERIFIED AND INSTALLED");
                        } else {
                            snprintf(update_message, sizeof(update_message),
                                "UPDATE FAILED - CURRENT VERSION KEPT");
                        }
                        redraw = 1;
                    }
                }

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
                    if (optimizer_result_load(&result)) {
                        screen = SCREEN_OPTIMIZER_RESULTS;
                        optimizer_message[0] = '\0';
                    } else {
                        snprintf(
                            optimizer_message,
                            sizeof(optimizer_message),
                            "NO RESULTS AVAILABLE");
                    }

                } else if (
                    optimizer_validation_selecting) {

                    optimizer_validation_selecting = 0;
                    optimizer_confirm = 3;
                    optimizer_message[0] = '\0';

                } else if (
                    optimizer_selected == 2) {

                    flip_optimizer_result_t result;
                    if (optimizer_result_load(&result)) {
                        optimizer_validation_selecting = 1;
                        optimizer_message[0] = '\0';
                    } else {
                        snprintf(
                            optimizer_message,
                            sizeof(optimizer_message),
                            "NO PROFILE TO VALIDATE");
                    }

                } else if (
                    optimizer_selected == 3) {

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

                int recent_game_result =
                    launch_game(
                        &drm,
                        &recent_roms[recent_selected]);

                if (recent_game_result < 0) {
                    fprintf(
                        stderr,
                        "FRONTEND_RECENT lifecycle failed\n");
                    return 3;
                }

                if (recent_game_result ==
                    FLIP_SESSION_ACTION_LOW_BATTERY) {
                    power_off_low_battery(&drm);
                    break;
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
                prev_y = 0;
                prev_left = 0;
                prev_right = 0;
                prev_start = 0;

                poweroff_active = 0;

                load_recent_history();
                refresh_play_stats();

                /*
                 * "Quit Game" is a deliberate end to the current session.
                 * GameSwitcher is reserved for suspended games, so a normal
                 * emulator exit always returns to Home.
                 */
                if (recent_game_result ==
                    FLIP_SESSION_ACTION_EXIT) {

                    screen =
                        SCREEN_HOME;

                    home_selected = 0;
                    message[0] = '\0';

                    fprintf(
                        stderr,
                        "FRONTEND_GAME quit_to_home\n");

                    draw_home(
                        &drm,
                        home_selected,
                        message,
                        0.0);

                    continue;
                }

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
                    centred_text_x("LAUNCHING", 3),
                    185,
                    "LAUNCHING",
                    3,
                    FG);

                flip_draw_text(
                    &drm,
                    centred_text_x(
                        roms[game_selected].display,
                        1),
                    235,
                    roms[game_selected].display,
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

                load_recent_history();
                refresh_play_stats();

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
                prev_y = 0;
                prev_left = 0;
                prev_right = 0;
                prev_start = 0;

                poweroff_active = 0;

                /*
                 * Rescan in case SD library changed.
                 */
                load_system_roms(
                    &drm,
                    game_system);

                if (game_result == FLIP_SESSION_ACTION_SUSPEND) {

                    load_recent_history();
                    refresh_play_stats();

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

                if (game_result ==
                    FLIP_SESSION_ACTION_EXIT) {

                    screen =
                        SCREEN_HOME;

                    home_selected = 0;
                    message[0] = '\0';

                    fprintf(
                        stderr,
                        "FRONTEND_GAME quit_to_home\n");

                    draw_home(
                        &drm,
                        home_selected,
                        message,
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
        prev_y = y_button;
        prev_left = left;
        prev_right = right;
        prev_start = start;

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
                    SCREEN_STATS) {

                draw_stats(
                    &drm,
                    progress);

            } else if (
                screen ==
                    SCREEN_GAME_DETAILS) {

                draw_game_details(
                    &drm,
                    progress);

            } else if (
                screen ==
                    SCREEN_SETTINGS) {

                draw_settings(
                    &drm,
                    settings_selected,
                    progress);

            } else if (screen == SCREEN_SETTINGS_CONNECTIVITY) {

                draw_settings_menu(
                    &drm, "CONNECTIVITY", connectivity_items,
                    CONNECTIVITY_ITEM_COUNT, connectivity_selected, 0, progress);

            } else if (screen == SCREEN_SETTINGS_SYSTEM) {

                draw_settings_menu(
                    &drm, "SYSTEM", system_settings_items,
                    SYSTEM_SETTINGS_ITEM_COUNT, system_settings_selected, 0, progress);

            } else if (screen == SCREEN_SETTINGS_TIME) {

                draw_time_settings(&drm, progress);

            } else if (screen == SCREEN_SETTINGS_BIOS) {

                draw_bios_verifier(&drm, progress);

            } else if (screen == SCREEN_SETTINGS_INTERFACE) {

                draw_settings_menu(
                    &drm, "INTERFACE", interface_items,
                    INTERFACE_ITEM_COUNT, interface_selected, 1, progress);

            } else if (screen == SCREEN_WIFI_PASSWORD) {

                draw_wifi_password(&drm, progress);

            } else if (screen == SCREEN_WIFI) {

                draw_wifi(&drm, wifi_selected, progress);

            } else if (first_boot_screen(screen)) {

                draw_first_boot(&drm, screen, progress);

            } else if (screen == SCREEN_UPDATE) {

                draw_update(&drm, progress);

            } else if (
                screen == SCREEN_OPTIMIZER_RESULTS) {

                draw_optimizer_results(&drm, progress);

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
                    SCREEN_STATS) {

                draw_stats(
                    &drm,
                    0.0);

            } else if (
                screen ==
                    SCREEN_GAME_DETAILS) {

                draw_game_details(
                    &drm,
                    0.0);

            } else if (
                screen ==
                    SCREEN_SETTINGS) {

                draw_settings(
                    &drm,
                    settings_selected,
                    0.0);

            } else if (screen == SCREEN_SETTINGS_CONNECTIVITY) {

                draw_settings_menu(
                    &drm, "CONNECTIVITY", connectivity_items,
                    CONNECTIVITY_ITEM_COUNT, connectivity_selected, 0, 0.0);

            } else if (screen == SCREEN_SETTINGS_SYSTEM) {

                draw_settings_menu(
                    &drm, "SYSTEM", system_settings_items,
                    SYSTEM_SETTINGS_ITEM_COUNT, system_settings_selected, 0, 0.0);

            } else if (screen == SCREEN_SETTINGS_TIME) {

                draw_time_settings(&drm, 0.0);

            } else if (screen == SCREEN_SETTINGS_BIOS) {

                draw_bios_verifier(&drm, 0.0);

            } else if (screen == SCREEN_SETTINGS_INTERFACE) {

                draw_settings_menu(
                    &drm, "INTERFACE", interface_items,
                    INTERFACE_ITEM_COUNT, interface_selected, 1, 0.0);

            } else if (screen == SCREEN_WIFI_PASSWORD) {

                draw_wifi_password(&drm, 0.0);

            } else if (screen == SCREEN_WIFI) {

                draw_wifi(&drm, wifi_selected, 0.0);

            } else if (first_boot_screen(screen)) {

                draw_first_boot(&drm, screen, 0.0);

            } else if (screen == SCREEN_UPDATE) {

                draw_update(&drm, 0.0);

            } else if (
                screen == SCREEN_OPTIMIZER_RESULTS) {

                draw_optimizer_results(&drm, 0.0);

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
