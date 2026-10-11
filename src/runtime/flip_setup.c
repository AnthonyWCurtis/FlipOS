#define _POSIX_C_SOURCE 200809L

#include "flip_setup.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int ensure_dir(void)
{
    char path[512];
    size_t length = strlen(FLIP_SETUP_ROOT);
    if (length == 0 || length >= sizeof(path))
        return -1;

    memcpy(path, FLIP_SETUP_ROOT, length + 1);
    for (char *p = path + 1; *p; ++p) {
        if (*p != '/')
            continue;
        *p = '\0';
        if (mkdir(path, 0755) < 0 && errno != EEXIST)
            return -1;
        *p = '/';
    }
    if (mkdir(path, 0755) < 0 && errno != EEXIST)
        return -1;
    return 0;
}

static int write_marker(const char *path, const char *text)
{
    if (ensure_dir() < 0)
        return -1;

    char tmp[640];
    int n = snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    if (n < 0 || (size_t)n >= sizeof(tmp))
        return -1;

    int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
        return -1;

    size_t length = strlen(text);
    int ok = write(fd, text, length) == (ssize_t)length && fsync(fd) == 0;
    int close_result = close(fd);
    if (!ok || close_result != 0) {
        unlink(tmp);
        return -1;
    }

    if (rename(tmp, path) < 0) {
        unlink(tmp);
        return -1;
    }
    return 0;
}

int flip_setup_is_complete(void)
{
    return access(FLIP_SETUP_COMPLETE_FILE, F_OK) == 0;
}

int flip_setup_is_in_progress(void)
{
    return access(FLIP_SETUP_PROGRESS_FILE, F_OK) == 0;
}

int flip_setup_begin(void)
{
    if (flip_setup_is_complete())
        return 0;
    return write_marker(FLIP_SETUP_PROGRESS_FILE, "version=1\n");
}

int flip_setup_mark_complete(void)
{
    if (write_marker(FLIP_SETUP_COMPLETE_FILE, "version=1\n") < 0)
        return -1;
    (void)unlink(FLIP_SETUP_PROGRESS_FILE);
    return 0;
}
