#define _POSIX_C_SOURCE 200809L

#include "flip_wifi_profiles.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int valid_ssid(const char *ssid)
{
    if (!ssid)
        return 0;
    size_t n = strlen(ssid);
    if (!n || n >= FLIP_WIFI_SSID_MAX)
        return 0;
    for (size_t i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)ssid[i];
        if (c < 0x20 || c == 0x7f)
            return 0;
    }
    return 1;
}

static int valid_psk(const char *psk)
{
    if (!psk || strlen(psk) != 64)
        return 0;
    for (int i = 0; i < 64; ++i) {
        if (!isxdigit((unsigned char)psk[i]))
            return 0;
    }
    return 1;
}

static int ensure_dir(void)
{
    char path[512];
    size_t len = strlen(FLIP_WIFI_CONFIG_DIR);
    if (len == 0 || len >= sizeof(path))
        return -1;
    memcpy(path, FLIP_WIFI_CONFIG_DIR, len + 1);
    for (char *p = path + 1; *p; ++p) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(path, 0700) < 0 && errno != EEXIST)
                return -1;
            *p = '/';
        }
    }
    if (mkdir(path, 0700) < 0 && errno != EEXIST)
        return -1;
    return chmod(path, 0700);
}

static char *trim(char *line)
{
    while (isspace((unsigned char)*line))
        ++line;
    size_t n = strlen(line);
    while (n && isspace((unsigned char)line[n - 1]))
        line[--n] = '\0';
    return line;
}

static int parse_quoted_ssid(const char *text, char *out)
{
    if (!text || *text++ != '"')
        return -1;
    size_t n = 0;
    while (*text && *text != '"') {
        unsigned char c = (unsigned char)*text++;
        if (c == '\\') {
            if (*text != '\\' && *text != '"')
                return -1;
            c = (unsigned char)*text++;
        }
        if (n + 1 >= FLIP_WIFI_SSID_MAX || c < 0x20 || c == 0x7f)
            return -1;
        out[n++] = (char)c;
    }
    if (*text != '"' || text[1] != '\0')
        return -1;
    out[n] = '\0';
    return valid_ssid(out) ? 0 : -1;
}

static int put_escaped_ssid(FILE *f, const char *ssid)
{
    if (fputc('"', f) == EOF)
        return -1;
    for (const unsigned char *p = (const unsigned char *)ssid; *p; ++p) {
        if ((*p == '\\' || *p == '"') && fputc('\\', f) == EOF)
            return -1;
        if (fputc(*p, f) == EOF)
            return -1;
    }
    return fputc('"', f) == EOF ? -1 : 0;
}

static void sort_profiles(flip_wifi_saved_profile_t *profiles, int count)
{
    /* Stable, descending priority; legacy profiles default to 0. */
    for (int i = 1; i < count; ++i) {
        flip_wifi_saved_profile_t current = profiles[i];
        int j = i;
        while (j > 0 && profiles[j - 1].priority < current.priority) {
            profiles[j] = profiles[j - 1];
            --j;
        }
        profiles[j] = current;
    }
}

int flip_wifi_profiles_load(
    flip_wifi_saved_profile_t profiles[FLIP_WIFI_SAVED_LIMIT])
{
    if (!profiles)
        return -1;

    FILE *f = fopen(FLIP_WIFI_CONFIG_FILE, "r");
    if (!f)
        return errno == ENOENT ? 0 : -1;

    int count = 0;
    int inside = 0;
    int ssid_seen = 0;
    int psk_seen = 0;
    int open_seen = 0;
    int failed = 0;
    flip_wifi_saved_profile_t current;
    memset(&current, 0, sizeof(current));

    char *line = NULL;
    size_t capacity = 0;
    ssize_t length;
    while ((length = getline(&line, &capacity, f)) >= 0) {
        if (length > 1024) {
            failed = 1;
            break;
        }
        char *p = trim(line);
        if (!*p || *p == '#')
            continue;

        if (!inside && strcmp(p, "network={") == 0) {
            if (count >= FLIP_WIFI_SAVED_LIMIT) {
                failed = 1;
                break;
            }
            memset(&current, 0, sizeof(current));
            inside = 1;
            ssid_seen = psk_seen = open_seen = 0;
            continue;
        }
        if (!inside) {
            if (strncmp(p, "ctrl_interface=", 15) == 0 ||
                strncmp(p, "update_config=", 14) == 0)
                continue;
            failed = 1; /* Refuse to discard unknown user configuration. */
            break;
        }

        if (strcmp(p, "}") == 0) {
            if (!ssid_seen || (psk_seen == open_seen)) {
                failed = 1;
                break;
            }
            for (int i = 0; i < count; ++i) {
                if (strcmp(profiles[i].ssid, current.ssid) == 0) {
                    failed = 1;
                    break;
                }
            }
            if (failed)
                break;
            current.open_network = open_seen;
            profiles[count++] = current;
            inside = 0;
            continue;
        }

        if (strncmp(p, "ssid=", 5) == 0 && !ssid_seen) {
            ssid_seen = parse_quoted_ssid(p + 5, current.ssid) == 0;
            if (!ssid_seen) {
                failed = 1;
                break;
            }
        } else if (strncmp(p, "psk=", 4) == 0 && !psk_seen) {
            if (!valid_psk(p + 4)) {
                failed = 1;
                break;
            }
            memcpy(current.psk, p + 4, 65);
            psk_seen = 1;
        } else if (strcmp(p, "key_mgmt=NONE") == 0 && !open_seen) {
            open_seen = 1;
        } else if (strncmp(p, "priority=", 9) == 0) {
            char *end = NULL;
            errno = 0;
            long value = strtol(p + 9, &end, 10);
            if (errno || !end || *end || value < 0 || value > 9999) {
                failed = 1;
                break;
            }
            current.priority = (int)value;
        } else {
            failed = 1;
            break;
        }
    }

    if (ferror(f) || inside)
        failed = 1;
    free(line);
    if (fclose(f) != 0)
        failed = 1;
    if (failed)
        return -1;
    sort_profiles(profiles, count);
    return count;
}

int flip_wifi_profiles_store(
    const flip_wifi_saved_profile_t *profiles,
    int count)
{
    if (count < 0 || count > FLIP_WIFI_SAVED_LIMIT ||
        (count && !profiles) || ensure_dir() < 0)
        return -1;

    for (int i = 0; i < count; ++i) {
        if (!valid_ssid(profiles[i].ssid) ||
            (!profiles[i].open_network && !valid_psk(profiles[i].psk)))
            return -1;
        for (int j = 0; j < i; ++j) {
            if (strcmp(profiles[i].ssid, profiles[j].ssid) == 0)
                return -1;
        }
    }

    char path[640];
    int n = snprintf(path, sizeof(path), "%s.XXXXXX", FLIP_WIFI_CONFIG_FILE);
    if (n < 0 || (size_t)n >= sizeof(path))
        return -1;

    int fd = mkstemp(path);
    if (fd < 0)
        return -1;
    if (fchmod(fd, 0600) != 0) {
        close(fd);
        unlink(path);
        return -1;
    }
    FILE *f = fdopen(fd, "w");
    if (!f) {
        close(fd);
        unlink(path);
        return -1;
    }

    int ok = fprintf(f,
        "ctrl_interface=/var/run/wpa_supplicant\n"
        "update_config=0\n") >= 0;

    for (int i = 0; ok && i < count; ++i) {
        ok = fprintf(f, "network={\n    ssid=") >= 0 &&
            put_escaped_ssid(f, profiles[i].ssid) == 0 &&
            fputc('\n', f) != EOF;
        if (profiles[i].open_network)
            ok = ok && fprintf(f, "    key_mgmt=NONE\n") >= 0;
        else
            ok = ok && fprintf(f, "    psk=%s\n", profiles[i].psk) >= 0;
        /* Priorities are regenerated from most recently successful first. */
        ok = ok && fprintf(f,
            "    priority=%d\n}\n", 1000 - i) >= 0;
    }

    if (fflush(f) != 0 || fsync(fd) != 0)
        ok = 0;
    if (fclose(f) != 0)
        ok = 0;
    if (!ok || rename(path, FLIP_WIFI_CONFIG_FILE) != 0) {
        unlink(path);
        return -1;
    }
    return 0;
}

int flip_wifi_profiles_upsert(
    const char *ssid,
    const char *hex_psk,
    int open_network)
{
    if (!valid_ssid(ssid) ||
        (!open_network && !valid_psk(hex_psk)))
        return -1;

    flip_wifi_saved_profile_t profiles[FLIP_WIFI_SAVED_LIMIT];
    int count = flip_wifi_profiles_load(profiles);
    if (count < 0)
        return -1;

    int found = -1;
    for (int i = 0; i < count; ++i) {
        if (strcmp(profiles[i].ssid, ssid) == 0) {
            found = i;
            break;
        }
    }
    if (found < 0 && count == FLIP_WIFI_SAVED_LIMIT)
        return -1;

    if (found >= 0) {
        for (int i = found; i > 0; --i)
            profiles[i] = profiles[i - 1];
    } else {
        for (int i = count; i > 0; --i)
            profiles[i] = profiles[i - 1];
        ++count;
    }

    flip_wifi_saved_profile_t *entry = &profiles[0];
    memset(entry, 0, sizeof(*entry));
    memcpy(entry->ssid, ssid, strlen(ssid) + 1);
    entry->open_network = !!open_network;
    if (!open_network)
        memcpy(entry->psk, hex_psk, 65);
    return flip_wifi_profiles_store(profiles, count);
}

int flip_wifi_profiles_forget(const char *ssid)
{
    if (!valid_ssid(ssid))
        return -1;
    flip_wifi_saved_profile_t profiles[FLIP_WIFI_SAVED_LIMIT];
    int count = flip_wifi_profiles_load(profiles);
    if (count < 0)
        return -1;
    int found = -1;
    for (int i = 0; i < count; ++i) {
        if (strcmp(profiles[i].ssid, ssid) == 0) {
            found = i;
            break;
        }
    }
    if (found < 0)
        return -1;
    for (int i = found; i < count - 1; ++i)
        profiles[i] = profiles[i + 1];
    return flip_wifi_profiles_store(profiles, count - 1);
}

int flip_wifi_profiles_promote(const char *ssid)
{
    if (!valid_ssid(ssid))
        return -1;
    flip_wifi_saved_profile_t profiles[FLIP_WIFI_SAVED_LIMIT];
    int count = flip_wifi_profiles_load(profiles);
    if (count < 0)
        return -1;
    int found = -1;
    for (int i = 0; i < count; ++i) {
        if (strcmp(profiles[i].ssid, ssid) == 0) {
            found = i;
            break;
        }
    }
    if (found < 0)
        return -1;
    if (found == 0)
        return 0;
    flip_wifi_saved_profile_t top = profiles[found];
    for (int i = found; i > 0; --i)
        profiles[i] = profiles[i - 1];
    profiles[0] = top;
    return flip_wifi_profiles_store(profiles, count);
}

int flip_wifi_profiles_contains(const char *ssid)
{
    if (!valid_ssid(ssid))
        return 0;
    flip_wifi_saved_profile_t profiles[FLIP_WIFI_SAVED_LIMIT];
    int count = flip_wifi_profiles_load(profiles);
    if (count < 0)
        return 0;
    for (int i = 0; i < count; ++i) {
        if (strcmp(profiles[i].ssid, ssid) == 0)
            return 1;
    }
    return 0;
}
