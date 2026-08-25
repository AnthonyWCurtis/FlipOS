#include "flip_library.h"

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

#define FLIP_LIBRARY_CACHE_ROOT \
    "/mnt/SDCARD/flipos-dev/cache"

#define FLIP_LIBRARY_CACHE_DIR \
    "/mnt/SDCARD/flipos-dev/cache/library"

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
