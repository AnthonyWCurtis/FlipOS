#include "flip_log.h"
#include "flip_paths.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static flip_log_level_t minimum_level = FLIP_LOG_DEBUG;
static FILE *log_stream;

static int valid_level(flip_log_level_t level)
{
    return level >= FLIP_LOG_DEBUG &&
           level <= FLIP_LOG_ERROR;
}

static int valid_component(const char *component)
{
    if (!component || !component[0])
        return 0;

    for (const unsigned char *p =
             (const unsigned char *)component;
         *p;
         ++p) {
        if ((*p >= 'a' && *p <= 'z') ||
            (*p >= 'A' && *p <= 'Z') ||
            (*p >= '0' && *p <= '9') ||
            *p == '_' ||
            *p == '-')
            continue;

        return 0;
    }

    return 1;
}

void flip_log_reset(void)
{
    minimum_level = FLIP_LOG_DEBUG;
    log_stream = NULL;
}

int flip_log_set_level(flip_log_level_t level)
{
    if (!valid_level(level))
        return -1;

    minimum_level = level;
    return 0;
}

flip_log_level_t flip_log_get_level(void)
{
    return minimum_level;
}

int flip_log_set_stream(FILE *stream)
{
    if (!stream)
        return -1;

    log_stream = stream;
    return 0;
}

int flip_log_path(
    const char *component,
    char *path,
    size_t path_size)
{
    if (!valid_component(component) ||
        !path ||
        path_size == 0)
        return -1;

    int written = snprintf(
        path,
        path_size,
        "%s/%s.log",
        flip_paths_log_root(),
        component);

    if (written < 0 ||
        (size_t)written >= path_size)
        return -1;

    return 0;
}

int flip_log_write(
    flip_log_level_t level,
    const char *component,
    const char *format,
    ...)
{
    if (!valid_level(level) ||
        !valid_component(component) ||
        !format)
        return -1;

    if (level < minimum_level)
        return 0;

    FILE *stream =
        log_stream
            ? log_stream
            : stderr;

    if (fprintf(
            stream,
            "%s: ",
            component) < 0)
        return -1;

    va_list args;
    va_start(args, format);
    int result =
        vfprintf(
            stream,
            format,
            args);
    va_end(args);

    if (result < 0)
        return -1;

    if (fputc('\n', stream) == EOF)
        return -1;

    return fflush(stream) == 0
        ? 0
        : -1;
}
