#ifndef FLIP_LOG_H
#define FLIP_LOG_H

#include <stddef.h>
#include <stdio.h>

typedef enum {
    FLIP_LOG_DEBUG = 0,
    FLIP_LOG_INFO,
    FLIP_LOG_WARN,
    FLIP_LOG_ERROR
} flip_log_level_t;

#define FLIP_LOG_PATH_MAX 512

void flip_log_reset(void);
int flip_log_set_level(flip_log_level_t level);
flip_log_level_t flip_log_get_level(void);
int flip_log_set_stream(FILE *stream);

int flip_log_path(
    const char *component,
    char *path,
    size_t path_size);

int flip_log_write(
    flip_log_level_t level,
    const char *component,
    const char *format,
    ...);

#endif
