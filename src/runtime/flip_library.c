#include "flip_library.h"
#include "flip_paths.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#define FLIP_LIBRARY_MAX_ENTRIES 4096

typedef struct {
    char name[512];
    off_t size;
    time_t mtime;
} flip_library_entry_t;

static uint64_t fnv1a_byte(
    uint64_t hash,
    unsigned char value)
{
    hash ^= value;
    hash *= UINT64_C(1099511628211);

    return hash;
}

static uint64_t fnv1a_data(
    uint64_t hash,
    const void *data,
    size_t size)
{
    const unsigned char *bytes =
        (const unsigned char *)data;

    for (size_t i = 0;
         i < size;
         ++i) {

        hash =
            fnv1a_byte(
                hash,
                bytes[i]);
    }

    return hash;
}

static uint64_t fnv1a_string(
    uint64_t hash,
    const char *text)
{
    if (!text)
        return hash;

    return fnv1a_data(
        hash,
        text,
        strlen(text));
}

static int compare_entries(
    const void *a,
    const void *b)
{
    const flip_library_entry_t *ea =
        (const flip_library_entry_t *)a;

    const flip_library_entry_t *eb =
        (const flip_library_entry_t *)b;

    return strcasecmp(
        ea->name,
        eb->name);
}


static int library_metadata_group(const char *text)
{
    static const char *exact[] = {
        "USA", "EUROPE", "JAPAN", "WORLD", "AUSTRALIA",
        "FRANCE", "GERMANY", "SPAIN", "ITALY", "KOREA",
        "CANADA", "BRAZIL", "CHINA", "TAIWAN",
        "EN", "FR", "DE", "ES", "IT", "JA", "KO", "TR",
        "BETA", "PROTO", "PROTOTYPE", "DEMO", "SAMPLE",
        "PREVIEW", "PRE-RELEASE"
    };

    if (!text || !text[0])
        return 0;

    char upper[96];
    size_t n = strlen(text);
    if (n >= sizeof(upper))
        return 0;

    for (size_t i = 0; i < n; ++i)
        upper[i] = (char)toupper((unsigned char)text[i]);
    upper[n] = '\0';

    for (size_t i = 0; i < sizeof(exact) / sizeof(exact[0]); ++i) {
        if (strcmp(upper, exact[i]) == 0)
            return 1;
    }

    if (strchr(upper, ',')) {
        char copy[96];
        snprintf(copy, sizeof(copy), "%s", upper);
        char *save = NULL;
        int parts = 0;
        for (char *part = strtok_r(copy, ",", &save);
             part;
             part = strtok_r(NULL, ",", &save)) {
            while (*part == ' ')
                ++part;
            char *tail = part + strlen(part);
            while (tail > part && tail[-1] == ' ')
                *--tail = '\0';
            if (!library_metadata_group(part))
                return 0;
            parts++;
        }
        return parts > 0;
    }

    return strncmp(upper, "REV ", 4) == 0 ||
        strncmp(upper, "REVISION ", 9) == 0 ||
        strncmp(upper, "DISC ", 5) == 0 ||
        strncmp(upper, "DISK ", 5) == 0 ||
        strncmp(upper, "SIDE ", 5) == 0 ||
        strncmp(upper, "PART ", 5) == 0;
}

static int library_dump_flag(const char *text)
{
    if (!text || !text[0])
        return 0;

    char upper[32];
    size_t n = strlen(text);
    if (n >= sizeof(upper))
        return 0;
    for (size_t i = 0; i < n; ++i)
        upper[i] = (char)toupper((unsigned char)text[i]);
    upper[n] = '\0';

    if (strcmp(upper, "!") == 0)
        return 1;

    static const char *prefixes[] = {"B", "H", "F", "T", "TR", "O", "U", "A"};
    for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); ++i) {
        size_t p = strlen(prefixes[i]);
        if (strncmp(upper, prefixes[i], p) != 0)
            continue;
        const char *rest = upper + p;
        if (!*rest)
            return 1;
        int numeric = 1;
        for (; *rest; ++rest) {
            if (!isdigit((unsigned char)*rest)) {
                numeric = 0;
                break;
            }
        }
        if (numeric)
            return 1;
    }
    return 0;
}

int flip_library_clean_display_title(
    const char *filename,
    char *out,
    size_t out_size)
{
    if (!filename || !out || out_size < 2)
        return -1;

    size_t n = strlen(filename);
    if (n >= out_size)
        n = out_size - 1;
    memcpy(out, filename, n);
    out[n] = '\0';

    char *slash = strrchr(out, '/');
    char *base = slash ? slash + 1 : out;
    if (slash)
        memmove(out, base, strlen(base) + 1);

    char *dot = strrchr(out, '.');
    if (dot && dot != out)
        *dot = '\0';

    for (;;) {
        n = strlen(out);
        while (n > 0 && isspace((unsigned char)out[n - 1]))
            out[--n] = '\0';
        if (n < 3)
            break;

        char close = out[n - 1];
        char open = close == ')' ? '(' : close == ']' ? '[' : '\0';
        if (!open)
            break;

        char *group = strrchr(out, open);
        if (!group || group >= out + n - 1)
            break;

        char metadata[96];
        size_t length = (size_t)((out + n - 1) - (group + 1));
        if (length == 0 || length >= sizeof(metadata))
            break;
        memcpy(metadata, group + 1, length);
        metadata[length] = '\0';

        int recognized = open == '['
            ? (library_dump_flag(metadata) || library_metadata_group(metadata))
            : library_metadata_group(metadata);
        if (!recognized)
            break;
        *group = '\0';
    }

    n = strlen(out);
    while (n > 0 && isspace((unsigned char)out[n - 1]))
        out[--n] = '\0';

    return out[0] ? 0 : -1;
}

int flip_library_fingerprint(
    const flip_system_t *system,
    uint64_t *fingerprint,
    int *rom_count)
{
    if (!system ||
        !system->rom_directory ||
        !fingerprint ||
        !rom_count) {

        return -1;
    }

    *fingerprint = 0;
    *rom_count = 0;

    DIR *dir =
        opendir(
            system->rom_directory);

    if (!dir) {

        if (errno == ENOENT)
            return 0;

        return -1;
    }

    flip_library_entry_t *entries =
        calloc(
            FLIP_LIBRARY_MAX_ENTRIES,
            sizeof(*entries));

    if (!entries) {

        closedir(dir);

        return -1;
    }

    int count = 0;

    struct dirent *entry;

    while ((entry = readdir(dir))) {

        if (entry->d_name[0] == '.')
            continue;

        char path[768];

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

        if (!flip_system_supports_path(
                system,
                path)) {

            continue;
        }

        struct stat st;

        if (stat(
                path,
                &st) < 0) {

            continue;
        }

        if (!S_ISREG(st.st_mode))
            continue;

        if (count >=
            FLIP_LIBRARY_MAX_ENTRIES) {

            fprintf(
                stderr,
                "LIBRARY: entry limit reached system=%s limit=%d\n",
                system->id
                    ? system->id
                    : "?",
                FLIP_LIBRARY_MAX_ENTRIES);

            break;
        }

        snprintf(
            entries[count].name,
            sizeof(entries[count].name),
            "%s",
            entry->d_name);

        entries[count].size =
            st.st_size;

        entries[count].mtime =
            st.st_mtime;

        count++;
    }

    closedir(dir);

    qsort(
        entries,
        count,
        sizeof(entries[0]),
        compare_entries);

    uint64_t hash =
        UINT64_C(14695981039346656037);

    /*
     * Include the owning system so two systems with the
     * same filenames/metadata cannot share a fingerprint.
     */
    hash =
        fnv1a_string(
            hash,
            system->id);

    hash =
        fnv1a_byte(
            hash,
            0xfe);

    for (int i = 0;
         i < count;
         ++i) {

        hash =
            fnv1a_string(
                hash,
                entries[i].name);

        hash =
            fnv1a_byte(
                hash,
                0xff);

        hash =
            fnv1a_data(
                hash,
                &entries[i].size,
                sizeof(entries[i].size));

        hash =
            fnv1a_data(
                hash,
                &entries[i].mtime,
                sizeof(entries[i].mtime));

        hash =
            fnv1a_byte(
                hash,
                0xfd);
    }

    hash =
        fnv1a_data(
            hash,
            &count,
            sizeof(count));

    free(entries);

    *fingerprint =
        hash;

    *rom_count =
        count;

    return 0;
}

static int ensure_cache_dir(void)
{
    if (mkdir(
            FLIP_LIBRARY_CACHE_ROOT,
            0777) < 0 &&
        errno != EEXIST) {

        return -1;
    }

    if (mkdir(
            FLIP_LIBRARY_CACHE_DIR,
            0777) < 0 &&
        errno != EEXIST) {

        return -1;
    }

    return 0;
}

static int fingerprint_path(
    const flip_system_t *system,
    char *path,
    size_t path_size)
{
    if (!system ||
        !system->id ||
        !path ||
        path_size == 0) {

        return -1;
    }

    int written =
        snprintf(
            path,
            path_size,
            "%s/%s.fingerprint",
            FLIP_LIBRARY_CACHE_DIR,
            system->id);

    if (written < 0 ||
        (size_t)written >= path_size) {

        return -1;
    }

    return 0;
}

int flip_library_load_fingerprint(
    const flip_system_t *system,
    uint64_t *fingerprint,
    int *rom_count)
{
    if (!system ||
        !fingerprint ||
        !rom_count) {

        return -1;
    }

    char path[512];

    if (fingerprint_path(
            system,
            path,
            sizeof(path)) < 0) {

        return -1;
    }

    FILE *f =
        fopen(
            path,
            "r");

    if (!f) {

        if (errno == ENOENT)
            return 0;

        return -1;
    }

    unsigned long long value = 0;
    int count = 0;
    int version = 0;

    char line[128];

    while (fgets(
               line,
               sizeof(line),
               f)) {

        if (sscanf(
                line,
                "version=%d",
                &version) == 1) {

            continue;
        }

        if (sscanf(
                line,
                "fingerprint=%llx",
                &value) == 1) {

            continue;
        }

        if (sscanf(
                line,
                "rom_count=%d",
                &count) == 1) {

            continue;
        }
    }

    fclose(f);

    if (version != 1 ||
        count < 0) {

        return -1;
    }

    *fingerprint =
        (uint64_t)value;

    *rom_count =
        count;

    return 1;
}

int flip_library_save_fingerprint(
    const flip_system_t *system,
    uint64_t fingerprint,
    int rom_count)
{
    if (!system ||
        rom_count < 0) {

        return -1;
    }

    if (ensure_cache_dir() < 0) {

        fprintf(
            stderr,
            "LIBRARY: cache mkdir failed: %s\n",
            strerror(errno));

        return -1;
    }

    char path[512];

    if (fingerprint_path(
            system,
            path,
            sizeof(path)) < 0) {

        return -1;
    }

    char tmp_path[544];

    int written =
        snprintf(
            tmp_path,
            sizeof(tmp_path),
            "%s.tmp",
            path);

    if (written < 0 ||
        (size_t)written >= sizeof(tmp_path)) {

        return -1;
    }

    FILE *f =
        fopen(
            tmp_path,
            "w");

    if (!f) {

        fprintf(
            stderr,
            "LIBRARY: cache fopen failed: %s\n",
            strerror(errno));

        return -1;
    }

    fprintf(
        f,
        "version=1\n");

    fprintf(
        f,
        "fingerprint=%016llx\n",
        (unsigned long long)
            fingerprint);

    fprintf(
        f,
        "rom_count=%d\n",
        rom_count);

    fflush(f);

    if (fsync(
            fileno(f)) < 0) {

        fprintf(
            stderr,
            "LIBRARY: cache fsync failed: %s\n",
            strerror(errno));

        fclose(f);
        unlink(tmp_path);

        return -1;
    }

    if (fclose(f) != 0) {

        unlink(tmp_path);

        return -1;
    }

    if (rename(
            tmp_path,
            path) < 0) {

        fprintf(
            stderr,
            "LIBRARY: cache rename failed: %s\n",
            strerror(errno));

        unlink(tmp_path);

        return -1;
    }

    fprintf(
        stderr,
        "LIBRARY: fingerprint saved system=%s roms=%d value=%016llx\n",
        system->id
            ? system->id
            : "?",
        rom_count,
        (unsigned long long)
            fingerprint);

    return 0;
}

static int index_path(
    const flip_system_t *system,
    char *path,
    size_t path_size)
{
    if (!system ||
        !system->id ||
        !path ||
        path_size == 0) {

        return -1;
    }

    int written =
        snprintf(
            path,
            path_size,
            "%s/%s.index",
            FLIP_LIBRARY_CACHE_DIR,
            system->id);

    if (written < 0 ||
        (size_t)written >= path_size) {

        return -1;
    }

    return 0;
}

static int validate_index_generation(
    const flip_system_t *system,
    int expected_live_count)
{
    if (!system || expected_live_count < 0)
        return -1;

    char path[512];

    if (index_path(
            system,
            path,
            sizeof(path)) < 0)
        return -1;

    FILE *f = fopen(path, "r");

    if (!f)
        return errno == ENOENT ? 0 : -1;

    char line[768];
    int version = 0;
    int declared_count = -1;
    int count = 0;
    int valid = 1;

    while (fgets(line, sizeof(line), f)) {
        size_t length = strlen(line);

        if (length > 0 && line[length - 1] == '\n')
            line[--length] = '\0';

        if (length > 0 && line[length - 1] == '\r')
            line[--length] = '\0';

        if (sscanf(line, "version=%d", &version) == 1)
            continue;

        if (sscanf(line, "rom_count=%d", &declared_count) == 1)
            continue;

        if (strncmp(line, "rom=", 4) != 0)
            continue;

        const char *name = line + 4;

        if (!name[0] ||
            strchr(name, '/') ||
            strlen(name) >= 512) {
            valid = 0;
            break;
        }

        char rom_path[768];
        int written = snprintf(
            rom_path,
            sizeof(rom_path),
            "%s/%s",
            system->rom_directory,
            name);

        if (written < 0 ||
            (size_t)written >= sizeof(rom_path) ||
            !flip_system_supports_path(system, rom_path)) {
            valid = 0;
            break;
        }

        count++;
    }

    if (ferror(f))
        valid = 0;

    if (fclose(f) != 0)
        valid = 0;

    if (!valid ||
        version != 1 ||
        declared_count < 0 ||
        declared_count != count ||
        declared_count != expected_live_count)
        return 0;

    return 1;
}

int flip_library_save_index(
    const flip_system_t *system,
    const char *const *names,
    int count)
{
    if (!system ||
        count < 0 ||
        (count > 0 && !names)) {

        return -1;
    }

    if (ensure_cache_dir() < 0) {

        fprintf(
            stderr,
            "LIBRARY: index mkdir failed: %s\n",
            strerror(errno));

        return -1;
    }

    char path[512];

    if (index_path(
            system,
            path,
            sizeof(path)) < 0) {

        return -1;
    }

    char tmp_path[544];

    int written =
        snprintf(
            tmp_path,
            sizeof(tmp_path),
            "%s.tmp",
            path);

    if (written < 0 ||
        (size_t)written >= sizeof(tmp_path)) {

        return -1;
    }

    FILE *f =
        fopen(
            tmp_path,
            "w");

    if (!f) {

        fprintf(
            stderr,
            "LIBRARY: index fopen failed: %s\n",
            strerror(errno));

        return -1;
    }

    fprintf(
        f,
        "version=1\n");

    fprintf(
        f,
        "rom_count=%d\n",
        count);

    for (int i = 0;
         i < count;
         ++i) {

        const char *name =
            names[i];

        if (!name ||
            !name[0] ||
            strchr(name, '\n') ||
            strchr(name, '\r') ||
            strchr(name, '/')) {

            fprintf(
                stderr,
                "LIBRARY: invalid index entry system=%s index=%d\n",
                system->id
                    ? system->id
                    : "?",
                i);

            fclose(f);
            unlink(tmp_path);

            return -1;
        }

        fprintf(
            f,
            "rom=%s\n",
            name);
    }

    fflush(f);

    if (fsync(
            fileno(f)) < 0) {

        fprintf(
            stderr,
            "LIBRARY: index fsync failed: %s\n",
            strerror(errno));

        fclose(f);
        unlink(tmp_path);

        return -1;
    }

    if (fclose(f) != 0) {

        unlink(tmp_path);
        return -1;
    }

    if (rename(
            tmp_path,
            path) < 0) {

        fprintf(
            stderr,
            "LIBRARY: index rename failed: %s\n",
            strerror(errno));

        unlink(tmp_path);

        return -1;
    }

    fprintf(
        stderr,
        "LIBRARY: index saved system=%s roms=%d\n",
        system->id
            ? system->id
            : "?",
        count);

    return 0;
}

int flip_library_cache_evaluate(
    const flip_system_t *system,
    flip_library_cache_status_t *status)
{
    if (!system || !status)
        return -1;

    memset(status, 0, sizeof(*status));
    status->state = FLIP_LIBRARY_CACHE_ERROR;

    if (flip_library_fingerprint(
            system,
            &status->live_fingerprint,
            &status->live_rom_count) < 0) {

        fprintf(
            stderr,
            "LIBRARY_CACHE: fingerprint failed system=%s\n",
            system->id ? system->id : "?");

        return -1;
    }

    int load_result =
        flip_library_load_fingerprint(
            system,
            &status->cached_fingerprint,
            &status->cached_rom_count);

    if (load_result <= 0) {
        if (load_result < 0) {
            fprintf(
                stderr,
                "LIBRARY_CACHE: invalid system=%s\n",
                system->id ? system->id : "?");
        }

        status->state = FLIP_LIBRARY_CACHE_MISS;

        fprintf(
            stderr,
            "LIBRARY_CACHE: MISS system=%s roms=%d\n",
            system->id ? system->id : "?",
            status->live_rom_count);

        return 0;
    }

    if (status->cached_fingerprint != status->live_fingerprint ||
        status->cached_rom_count != status->live_rom_count) {

        status->state = FLIP_LIBRARY_CACHE_CHANGED;

        fprintf(
            stderr,
            "LIBRARY_CACHE: CHANGED system=%s old_roms=%d new_roms=%d\n",
            system->id ? system->id : "?",
            status->cached_rom_count,
            status->live_rom_count);

        return 0;
    }

    int index_valid =
        validate_index_generation(
            system,
            status->live_rom_count);

    if (index_valid < 0) {
        status->state = FLIP_LIBRARY_CACHE_ERROR;
        return -1;
    }

    if (index_valid == 0) {
        status->state = FLIP_LIBRARY_CACHE_MISS;

        fprintf(
            stderr,
            "LIBRARY_CACHE: index invalid system=%s rebuilding\n",
            system->id ? system->id : "?");

        return 0;
    }

    status->state = FLIP_LIBRARY_CACHE_HIT;

    fprintf(
        stderr,
        "LIBRARY_CACHE: HIT system=%s roms=%d\n",
        system->id ? system->id : "?",
        status->live_rom_count);

    return 0;
}

int flip_library_commit_generation(
    const flip_system_t *system,
    const char *const *names,
    int count)
{
    if (!system ||
        count < 0 ||
        (count > 0 && !names))
        return -1;

    if (flip_library_save_index(
            system,
            names,
            count) < 0) {

        fprintf(
            stderr,
            "LIBRARY_CACHE: index save failed system=%s\n",
            system->id ? system->id : "?");

        return -1;
    }

    uint64_t fingerprint = 0;
    int fingerprint_count = 0;

    if (flip_library_fingerprint(
            system,
            &fingerprint,
            &fingerprint_count) < 0 ||
        fingerprint_count != count) {

        fprintf(
            stderr,
            "LIBRARY_CACHE: fingerprint commit failed system=%s\n",
            system->id ? system->id : "?");

        return -1;
    }

    if (flip_library_save_fingerprint(
            system,
            fingerprint,
            fingerprint_count) < 0) {

        fprintf(
            stderr,
            "LIBRARY_CACHE: fingerprint save failed system=%s\n",
            system->id ? system->id : "?");

        return -1;
    }

    fprintf(
        stderr,
        "LIBRARY_CACHE: index rebuilt system=%s roms=%d\n",
        system->id ? system->id : "?",
        count);

    return 0;
}

int flip_library_load_index(
    const flip_system_t *system,
    char *names,
    int max_names,
    size_t name_size)
{
    if (!system ||
        !names ||
        max_names <= 0 ||
        name_size < 2) {

        return -1;
    }

    char path[512];

    if (index_path(
            system,
            path,
            sizeof(path)) < 0) {

        return -1;
    }

    FILE *f =
        fopen(
            path,
            "r");

    if (!f)
        return -1;

    char line[768];

    int version = 0;
    int expected_count = -1;
    int count = 0;

    while (fgets(
               line,
               sizeof(line),
               f)) {

        size_t length =
            strlen(line);

        if (length > 0 &&
            line[length - 1] == '\n') {

            line[--length] = '\0';
        }

        if (length > 0 &&
            line[length - 1] == '\r') {

            line[--length] = '\0';
        }

        if (sscanf(
                line,
                "version=%d",
                &version) == 1) {

            continue;
        }

        if (sscanf(
                line,
                "rom_count=%d",
                &expected_count) == 1) {

            continue;
        }

        if (strncmp(
                line,
                "rom=",
                4) != 0) {

            continue;
        }

        const char *name =
            line + 4;

        if (!name[0] ||
            strchr(name, '/') ||
            count >= max_names ||
            strlen(name) >= name_size) {

            fclose(f);
            return -1;
        }

        char *destination =
            names +
            ((size_t)count *
             name_size);

        snprintf(
            destination,
            name_size,
            "%s",
            name);

        count++;
    }

    if (ferror(f)) {

        fclose(f);
        return -1;
    }

    fclose(f);

    if (version != 1 ||
        expected_count < 0 ||
        expected_count != count) {

        return -1;
    }

    fprintf(
        stderr,
        "LIBRARY: index loaded system=%s roms=%d\n",
        system->id
            ? system->id
            : "?",
        count);

    return count;
}
