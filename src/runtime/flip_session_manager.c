#define _GNU_SOURCE

#include "flip_session_manager.h"
#include "flip_system.h"

#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static uint64_t fnv1a64(const char *text)
{
    uint64_t hash = UINT64_C(14695981039346656037);

    while (text && *text) {
        hash ^= (unsigned char)*text++;
        hash *= UINT64_C(1099511628211);
    }

    return hash;
}

static int valid_system_id(const char *system_id)
{
    if (!system_id || !system_id[0])
        return 0;

    for (const char *p = system_id; *p; ++p) {
        unsigned char c = (unsigned char)*p;
        if (!((c >= 'A' && c <= 'Z') ||
              (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') ||
              c == '_' || c == '-'))
            return 0;
    }

    return 1;
}

static int copy_string(char *dest, size_t dest_size, const char *source)
{
    if (!dest || dest_size == 0 || !source)
        return -1;

    size_t length = strlen(source);
    if (length >= dest_size)
        return -1;

    memcpy(dest, source, length + 1);
    return 0;
}

static int ensure_directory(const char *path)
{
    if (mkdir(path, 0777) == 0 || errno == EEXIST)
        return 0;
    return -1;
}

static int ensure_root(void)
{
    return ensure_directory(FLIP_SESSION_MANAGER_ROOT);
}

static int build_system_dir(
    char *out,
    size_t out_size,
    const char *system_id)
{
    if (!out || out_size == 0 || !valid_system_id(system_id))
        return -1;

    int written = snprintf(
        out,
        out_size,
        "%s/%s",
        FLIP_SESSION_MANAGER_ROOT,
        system_id);

    if (written < 0 || (size_t)written >= out_size)
        return -1;

    return 0;
}

static int build_session_path(
    char *out,
    size_t out_size,
    const char *system_id,
    const char *rom_path)
{
    if (!out || out_size == 0 || !valid_system_id(system_id) ||
        !rom_path || !rom_path[0])
        return -1;

    int written = snprintf(
        out,
        out_size,
        "%s/%s/%016llx.session",
        FLIP_SESSION_MANAGER_ROOT,
        system_id,
        (unsigned long long)fnv1a64(rom_path));

    if (written < 0 || (size_t)written >= out_size)
        return -1;

    return 0;
}

static const char *status_name(flip_retained_session_status_t status)
{
    switch (status) {
    case FLIP_RETAINED_SESSION_SUSPENDED:
        return "suspended";
    case FLIP_RETAINED_SESSION_INTERRUPTED:
        return "interrupted";
    default:
        return NULL;
    }
}

static int parse_status(
    const char *text,
    flip_retained_session_status_t *status)
{
    if (!text || !status)
        return -1;

    if (strcmp(text, "suspended") == 0) {
        *status = FLIP_RETAINED_SESSION_SUSPENDED;
        return 0;
    }

    if (strcmp(text, "interrupted") == 0) {
        *status = FLIP_RETAINED_SESSION_INTERRUPTED;
        return 0;
    }

    return -1;
}

static int parse_long_long(const char *text, long long *value)
{
    if (!text || !text[0] || !value)
        return -1;

    errno = 0;
    char *end = NULL;
    long long parsed = strtoll(text, &end, 10);

    if (errno != 0 || !end || *end != '\0')
        return -1;

    *value = parsed;
    return 0;
}

int flip_session_manager_record_gameswitcher(
    const flip_system_t *system,
    const char *rom_path)
{
    if (!system ||
        !system->id ||
        !rom_path ||
        !rom_path[0])
        return -1;

    return flip_session_manager_record(
        system->id,
        rom_path,
        FLIP_RETAINED_SESSION_SUSPENDED,
        (long long)time(NULL));
}

int flip_session_manager_record(
    const char *system_id,
    const char *rom_path,
    flip_retained_session_status_t status,
    long long last_active)
{
    const char *status_text = status_name(status);
    if (!status_text || !valid_system_id(system_id) ||
        !rom_path || !rom_path[0] ||
        strlen(system_id) >= sizeof(((flip_retained_session_t *)0)->system) ||
        strlen(rom_path) >= sizeof(((flip_retained_session_t *)0)->rom))
        return -1;

    if (ensure_root() < 0)
        return -1;

    char system_dir[768];
    if (build_system_dir(system_dir, sizeof(system_dir), system_id) < 0)
        return -1;

    if (ensure_directory(system_dir) < 0)
        return -1;

    char path[1024];
    if (build_session_path(path, sizeof(path), system_id, rom_path) < 0)
        return -1;

    char tmp_path[1060];
    int written = snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    if (written < 0 || (size_t)written >= sizeof(tmp_path))
        return -1;

    FILE *f = fopen(tmp_path, "w");
    if (!f)
        return -1;

    int ok = 1;
    ok = ok && fprintf(f, "version=1\n") >= 0;
    ok = ok && fprintf(f, "system=%s\n", system_id) >= 0;
    ok = ok && fprintf(f, "rom=%s\n", rom_path) >= 0;
    ok = ok && fprintf(f, "status=%s\n", status_text) >= 0;
    ok = ok && fprintf(f, "last_active=%lld\n", last_active) >= 0;
    ok = ok && fflush(f) == 0;
    ok = ok && fsync(fileno(f)) == 0;

    if (fclose(f) != 0)
        ok = 0;

    if (!ok) {
        unlink(tmp_path);
        return -1;
    }

    if (rename(tmp_path, path) < 0) {
        unlink(tmp_path);
        return -1;
    }

    return 0;
}

static int has_session_suffix(const char *name)
{
    static const char suffix[] = ".session";
    size_t name_len = name ? strlen(name) : 0;
    size_t suffix_len = sizeof(suffix) - 1;

    return name_len > suffix_len &&
        strcmp(name + name_len - suffix_len, suffix) == 0;
}

static int read_session_file(
    const char *path,
    flip_retained_session_t *session)
{
    if (!path || !session)
        return -1;

    FILE *f = fopen(path, "r");
    if (!f)
        return -1;

    char system[32] = "";
    char rom[512] = "";
    char status_text[32] = "";
    long long last_active = 0;
    int version = 0;
    int have_version = 0;
    int have_system = 0;
    int have_rom = 0;
    int have_status = 0;
    int have_last_active = 0;

    char line[768];
    while (fgets(line, sizeof(line), f)) {
        char *newline = strchr(line, '\n');
        if (newline)
            *newline = '\0';

        char *equals = strchr(line, '=');
        if (!equals)
            continue;

        *equals = '\0';
        const char *key = line;
        const char *value = equals + 1;

        if (strcmp(key, "version") == 0) {
            char *end = NULL;
            errno = 0;
            long parsed = strtol(value, &end, 10);
            if (errno == 0 && end && *end == '\0') {
                version = (int)parsed;
                have_version = 1;
            }
        } else if (strcmp(key, "system") == 0) {
            if (copy_string(system, sizeof(system), value) == 0)
                have_system = 1;
        } else if (strcmp(key, "rom") == 0) {
            if (copy_string(rom, sizeof(rom), value) == 0)
                have_rom = 1;
        } else if (strcmp(key, "status") == 0) {
            if (copy_string(status_text, sizeof(status_text), value) == 0)
                have_status = 1;
        } else if (strcmp(key, "last_active") == 0) {
            if (parse_long_long(value, &last_active) == 0)
                have_last_active = 1;
        }
    }

    int close_ok = fclose(f) == 0;
    if (!close_ok || !have_version || version != 1 ||
        !have_system || !valid_system_id(system) ||
        !have_rom || !rom[0] ||
        !have_status || !have_last_active)
        return -1;

    flip_retained_session_status_t status;
    if (parse_status(status_text, &status) < 0)
        return -1;

    memset(session, 0, sizeof(*session));
    if (copy_string(session->system, sizeof(session->system), system) < 0 ||
        copy_string(session->rom, sizeof(session->rom), rom) < 0)
        return -1;

    session->status = status;
    session->last_active = last_active;
    session->resume_available = 0;
    return 0;
}

static int append_session(
    flip_retained_session_list_t *list,
    const flip_retained_session_t *session)
{
    size_t new_count = list->count + 1;
    if (new_count < list->count)
        return -1;

    flip_retained_session_t *items = realloc(
        list->items,
        new_count * sizeof(*items));
    if (!items)
        return -1;

    list->items = items;
    list->items[list->count] = *session;
    list->count = new_count;
    return 0;
}

static int compare_recent_first(const void *a, const void *b)
{
    const flip_retained_session_t *left = a;
    const flip_retained_session_t *right = b;

    if (left->last_active > right->last_active)
        return -1;
    if (left->last_active < right->last_active)
        return 1;

    int system_cmp = strcmp(left->system, right->system);
    if (system_cmp != 0)
        return system_cmp;

    return strcmp(left->rom, right->rom);
}

int flip_session_manager_load(flip_retained_session_list_t *list)
{
    if (!list)
        return -1;

    flip_session_manager_list_free(list);

    DIR *root = opendir(FLIP_SESSION_MANAGER_ROOT);
    if (!root) {
        if (errno == ENOENT)
            return 0;
        return -1;
    }

    struct dirent *system_entry;
    while ((system_entry = readdir(root)) != NULL) {
        if (system_entry->d_name[0] == '.')
            continue;
        if (!valid_system_id(system_entry->d_name))
            continue;

        char system_dir[768];
        if (build_system_dir(
                system_dir,
                sizeof(system_dir),
                system_entry->d_name) < 0)
            continue;

        struct stat system_st;
        if (stat(system_dir, &system_st) < 0 || !S_ISDIR(system_st.st_mode))
            continue;

        DIR *dir = opendir(system_dir);
        if (!dir) {
            closedir(root);
            flip_session_manager_list_free(list);
            return -1;
        }

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (!has_session_suffix(entry->d_name))
                continue;

            char path[1024];
            int written = snprintf(
                path,
                sizeof(path),
                "%s/%s",
                system_dir,
                entry->d_name);
            if (written < 0 || (size_t)written >= sizeof(path))
                continue;

            struct stat st;
            if (stat(path, &st) < 0 || !S_ISREG(st.st_mode))
                continue;

            flip_retained_session_t session;
            if (read_session_file(path, &session) < 0)
                continue;

            if (strcmp(session.system, system_entry->d_name) != 0)
                continue;

            if (append_session(list, &session) < 0) {
                closedir(dir);
                closedir(root);
                flip_session_manager_list_free(list);
                return -1;
            }
        }

        closedir(dir);
    }

    closedir(root);

    if (list->count > 1) {
        qsort(
            list->items,
            list->count,
            sizeof(*list->items),
            compare_recent_first);
    }

    return 0;
}


int flip_session_manager_refresh(
    flip_retained_session_list_t *list)
{
    if (!list)
        return -1;

    if (flip_session_manager_load(list) < 0)
        return -1;

    size_t write_index = 0;

    for (size_t i = 0; i < list->count; ++i) {
        flip_retained_session_t *session = &list->items[i];

        const flip_system_t *system =
            flip_system_find(session->system);

        if (!system) {
            flip_session_manager_remove(
                session->system,
                session->rom);
            continue;
        }

        struct stat rom_st;
        if (stat(session->rom, &rom_st) < 0 ||
            !S_ISREG(rom_st.st_mode) ||
            access(session->rom, R_OK) < 0) {

            flip_session_manager_remove(
                session->system,
                session->rom);
            continue;
        }

        if (!flip_system_supports_path(
                system,
                session->rom)) {

            flip_session_manager_remove(
                session->system,
                session->rom);
            continue;
        }

        session->resume_available =
            flip_system_resume_available(
                system,
                session->rom);

        int duplicate = 0;

        for (size_t j = 0; j < write_index; ++j) {
            if (strcmp(
                    list->items[j].system,
                    session->system) == 0 &&
                strcmp(
                    list->items[j].rom,
                    session->rom) == 0) {

                duplicate = 1;
                break;
            }
        }

        if (duplicate)
            continue;

        if (write_index != i)
            list->items[write_index] = *session;

        ++write_index;
    }

    list->count = write_index;

    if (list->count == 0) {
        free(list->items);
        list->items = NULL;
    }

    return 0;
}


int flip_session_manager_prune(void)
{
    flip_retained_session_list_t list;
    memset(&list, 0, sizeof(list));

    if (flip_session_manager_refresh(&list) < 0) {
        flip_session_manager_list_free(&list);
        return -1;
    }

    flip_session_manager_list_free(&list);
    return 0;
}

void flip_session_manager_list_free(flip_retained_session_list_t *list)
{
    if (!list)
        return;

    free(list->items);
    list->items = NULL;
    list->count = 0;
}

int flip_session_manager_remove(
    const char *system_id,
    const char *rom_path)
{
    char path[1024];
    if (build_session_path(path, sizeof(path), system_id, rom_path) < 0)
        return -1;

    if (unlink(path) == 0)
        return 1;

    if (errno == ENOENT)
        return 0;

    return -1;
}
