#define _GNU_SOURCE

#include "flip_session_registry.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef FLIP_SESSION_REGISTRY_ROOT
#define FLIP_SESSION_REGISTRY_ROOT "/mnt/SDCARD/flipos-dev/state/session"
#endif

#define REGISTRY_FILE FLIP_SESSION_REGISTRY_ROOT "/registry"
#define REGISTRY_VERSION 1

static int copy_string(
    char *dest,
    size_t dest_size,
    const char *source)
{
    if (!dest || dest_size == 0)
        return -1;

    if (!source) {
        dest[0] = '\0';
        return 0;
    }

    size_t length = strlen(source);

    if (length >= dest_size)
        return -1;

    memcpy(dest, source, length + 1);
    return 0;
}

static int text_is_safe(const char *text)
{
    return text &&
           text[0] &&
           !strchr(text, '\n') &&
           !strchr(text, '\r');
}

static int parse_ll(
    const char *text,
    long long *value)
{
    if (!text || !text[0] || !value)
        return -1;

    errno = 0;

    char *end = NULL;
    long long parsed = strtoll(text, &end, 10);

    if (errno != 0 ||
        !end ||
        *end != '\0' ||
        parsed < 0)
        return -1;

    *value = parsed;
    return 0;
}

static int validate_entry(
    const flip_session_registry_entry_t *entry)
{
    if (!entry ||
        !text_is_safe(entry->system) ||
        !text_is_safe(entry->rom) ||
        !text_is_safe(entry->program) ||
        !text_is_safe(entry->state) ||
        entry->started < 0 ||
        entry->updated < 0)
        return -1;

    return 0;
}

static int ensure_root(void)
{
    if (mkdir(
            FLIP_SESSION_REGISTRY_ROOT,
            0777) < 0 &&
        errno != EEXIST)
        return -1;

    return 0;
}

static int write_registry(
    const flip_session_registry_entry_t *entries,
    size_t count)
{
    if (ensure_root() < 0) {
        fprintf(
            stderr,
            "SESSION_REGISTRY: mkdir failed: %s\n",
            strerror(errno));
        return -1;
    }

    char tmp_path[1024];

    int written = snprintf(
        tmp_path,
        sizeof(tmp_path),
        "%s.tmp",
        REGISTRY_FILE);

    if (written < 0 ||
        (size_t)written >= sizeof(tmp_path))
        return -1;

    FILE *f = fopen(tmp_path, "w");

    if (!f) {
        fprintf(
            stderr,
            "SESSION_REGISTRY: fopen failed: %s\n",
            strerror(errno));
        return -1;
    }

    fprintf(f, "version=%d\n", REGISTRY_VERSION);

    for (size_t i = 0; i < count; ++i) {
        if (validate_entry(&entries[i]) < 0) {
            fclose(f);
            unlink(tmp_path);
            return -1;
        }

        fprintf(f, "entry\n");
        fprintf(f, "system=%s\n", entries[i].system);
        fprintf(f, "rom=%s\n", entries[i].rom);
        fprintf(f, "program=%s\n", entries[i].program);
        fprintf(f, "state=%s\n", entries[i].state);
        fprintf(f, "started=%lld\n", entries[i].started);
        fprintf(f, "updated=%lld\n", entries[i].updated);
        fprintf(f, "end\n");
    }

    int write_failed = 0;
    int saved_errno = 0;

    if (fflush(f) != 0) {
        write_failed = 1;
        saved_errno = errno;
    } else if (fsync(fileno(f)) < 0) {
        write_failed = 1;
        saved_errno = errno;
    }

    if (fclose(f) != 0 && !write_failed) {
        write_failed = 1;
        saved_errno = errno;
    }

    if (write_failed) {
        unlink(tmp_path);
        errno = saved_errno;

        fprintf(
            stderr,
            "SESSION_REGISTRY: flush failed: %s\n",
            strerror(errno));
        return -1;
    }

    if (rename(tmp_path, REGISTRY_FILE) < 0) {
        fprintf(
            stderr,
            "SESSION_REGISTRY: rename failed: %s\n",
            strerror(errno));
        unlink(tmp_path);
        return -1;
    }

    return 0;
}

int flip_session_registry_load(
    flip_session_registry_entry_t *entries,
    size_t capacity,
    size_t *count)
{
    if (!count ||
        (capacity > 0 && !entries))
        return -1;

    *count = 0;

    FILE *f = fopen(REGISTRY_FILE, "r");

    if (!f) {
        if (errno == ENOENT)
            return 0;

        fprintf(
            stderr,
            "SESSION_REGISTRY: read failed: %s\n",
            strerror(errno));
        return -1;
    }

    char line[1024];

    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return -1;
    }

    char *newline = strchr(line, '\n');
    if (newline)
        *newline = '\0';

    if (strcmp(line, "version=1") != 0) {
        fclose(f);
        return -1;
    }

    flip_session_registry_entry_t current;
    memset(&current, 0, sizeof(current));

    int in_entry = 0;
    unsigned fields = 0;

    while (fgets(line, sizeof(line), f)) {
        newline = strchr(line, '\n');

        if (newline) {
            *newline = '\0';
        } else if (!feof(f)) {
            fclose(f);
            return -1;
        }

        if (strcmp(line, "entry") == 0) {
            if (in_entry) {
                fclose(f);
                return -1;
            }

            memset(&current, 0, sizeof(current));
            in_entry = 1;
            fields = 0;
            continue;
        }

        if (strcmp(line, "end") == 0) {
            if (!in_entry ||
                fields != 0x3f ||
                validate_entry(&current) < 0) {
                fclose(f);
                return -1;
            }

            current.valid = 1;

            if (*count < capacity)
                entries[*count] = current;

            (*count)++;

            if (*count > FLIP_SESSION_REGISTRY_MAX) {
                fclose(f);
                return -1;
            }

            in_entry = 0;
            continue;
        }

        if (!in_entry) {
            if (line[0] == '\0')
                continue;

            fclose(f);
            return -1;
        }

        if (strncmp(line, "system=", 7) == 0) {
            if ((fields & 0x01) ||
                copy_string(
                    current.system,
                    sizeof(current.system),
                    line + 7) < 0) {
                fclose(f);
                return -1;
            }
            fields |= 0x01;
        } else if (strncmp(line, "rom=", 4) == 0) {
            if ((fields & 0x02) ||
                copy_string(
                    current.rom,
                    sizeof(current.rom),
                    line + 4) < 0) {
                fclose(f);
                return -1;
            }
            fields |= 0x02;
        } else if (strncmp(line, "program=", 8) == 0) {
            if ((fields & 0x04) ||
                copy_string(
                    current.program,
                    sizeof(current.program),
                    line + 8) < 0) {
                fclose(f);
                return -1;
            }
            fields |= 0x04;
        } else if (strncmp(line, "state=", 6) == 0) {
            if ((fields & 0x08) ||
                copy_string(
                    current.state,
                    sizeof(current.state),
                    line + 6) < 0) {
                fclose(f);
                return -1;
            }
            fields |= 0x08;
        } else if (strncmp(line, "started=", 8) == 0) {
            if ((fields & 0x10) ||
                parse_ll(
                    line + 8,
                    &current.started) < 0) {
                fclose(f);
                return -1;
            }
            fields |= 0x10;
        } else if (strncmp(line, "updated=", 8) == 0) {
            if ((fields & 0x20) ||
                parse_ll(
                    line + 8,
                    &current.updated) < 0) {
                fclose(f);
                return -1;
            }
            fields |= 0x20;
        } else {
            fclose(f);
            return -1;
        }
    }

    fclose(f);

    if (in_entry)
        return -1;

    if (*count > capacity)
        return -1;

    return 0;
}

int flip_session_registry_upsert(
    const flip_session_registry_entry_t *entry)
{
    if (validate_entry(entry) < 0)
        return -1;

    flip_session_registry_entry_t entries[
        FLIP_SESSION_REGISTRY_MAX];

    size_t count = 0;

    if (flip_session_registry_load(
            entries,
            FLIP_SESSION_REGISTRY_MAX,
            &count) < 0)
        return -1;

    flip_session_registry_entry_t next[
        FLIP_SESSION_REGISTRY_MAX];

    size_t next_count = 0;
    next[next_count++] = *entry;
    next[0].valid = 1;

    for (size_t i = 0;
         i < count &&
         next_count < FLIP_SESSION_REGISTRY_MAX;
         ++i) {

        if (strcmp(
                entries[i].system,
                entry->system) == 0 &&
            strcmp(
                entries[i].rom,
                entry->rom) == 0)
            continue;

        next[next_count++] = entries[i];
    }

    if (write_registry(next, next_count) < 0)
        return -1;

    fprintf(
        stderr,
        "SESSION_REGISTRY: upsert state=%s system=%s rom=%s count=%zu\n",
        entry->state,
        entry->system,
        entry->rom,
        next_count);

    return 0;
}

int flip_session_registry_remove(
    const char *system,
    const char *rom)
{
    if (!text_is_safe(system) ||
        !text_is_safe(rom))
        return -1;

    flip_session_registry_entry_t entries[
        FLIP_SESSION_REGISTRY_MAX];

    size_t count = 0;

    if (flip_session_registry_load(
            entries,
            FLIP_SESSION_REGISTRY_MAX,
            &count) < 0)
        return -1;

    flip_session_registry_entry_t next[
        FLIP_SESSION_REGISTRY_MAX];

    size_t next_count = 0;
    int removed = 0;

    for (size_t i = 0; i < count; ++i) {
        if (strcmp(entries[i].system, system) == 0 &&
            strcmp(entries[i].rom, rom) == 0) {
            removed = 1;
            continue;
        }

        next[next_count++] = entries[i];
    }

    if (!removed)
        return 0;

    if (write_registry(next, next_count) < 0)
        return -1;

    fprintf(
        stderr,
        "SESSION_REGISTRY: removed system=%s rom=%s count=%zu\n",
        system,
        rom,
        next_count);

    return 1;
}

int flip_session_registry_find(
    const char *system,
    const char *rom,
    flip_session_registry_entry_t *entry)
{
    if (!text_is_safe(system) ||
        !text_is_safe(rom) ||
        !entry)
        return -1;

    flip_session_registry_entry_t entries[
        FLIP_SESSION_REGISTRY_MAX];

    size_t count = 0;

    if (flip_session_registry_load(
            entries,
            FLIP_SESSION_REGISTRY_MAX,
            &count) < 0)
        return -1;

    for (size_t i = 0; i < count; ++i) {
        if (strcmp(entries[i].system, system) == 0 &&
            strcmp(entries[i].rom, rom) == 0) {
            *entry = entries[i];
            return 1;
        }
    }

    memset(entry, 0, sizeof(*entry));
    return 0;
}
