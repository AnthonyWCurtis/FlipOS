#include "flip_log.h"
#include "flip_paths.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "CHECK failed: %s:%d: %s\n", \
            __FILE__, __LINE__, #condition); \
        failures++; \
    } \
} while (0)

static void read_stream(
    FILE *stream,
    char *buffer,
    size_t buffer_size)
{
    fflush(stream);
    rewind(stream);

    size_t count =
        fread(
            buffer,
            1,
            buffer_size - 1,
            stream);

    buffer[count] = '\0';
}

int main(void)
{
    flip_log_reset();

    CHECK(flip_log_get_level() == FLIP_LOG_DEBUG);
    CHECK(flip_log_set_level(FLIP_LOG_INFO) == 0);
    CHECK(flip_log_get_level() == FLIP_LOG_INFO);
    CHECK(flip_log_set_level((flip_log_level_t)99) < 0);

    char path[FLIP_LOG_PATH_MAX];
    CHECK(flip_log_path("frontend", path, sizeof(path)) == 0);
    CHECK(strcmp(
        path,
        FLIP_LOG_ROOT "/frontend.log") == 0);
    CHECK(flip_log_path("", path, sizeof(path)) < 0);
    CHECK(flip_log_path("bad/name", path, sizeof(path)) < 0);

    char tiny[8];
    CHECK(flip_log_path("frontend", tiny, sizeof(tiny)) < 0);

    FILE *stream = tmpfile();
    CHECK(stream != NULL);

    if (stream) {
        CHECK(flip_log_set_stream(stream) == 0);

        CHECK(flip_log_write(
            FLIP_LOG_DEBUG,
            "TEST",
            "hidden") == 0);

        CHECK(flip_log_write(
            FLIP_LOG_INFO,
            "TEST",
            "value=%d",
            7) == 0);

        char output[128];
        read_stream(
            stream,
            output,
            sizeof(output));

        CHECK(strcmp(
            output,
            "TEST: value=7\n") == 0);

        fclose(stream);
    }

    CHECK(flip_log_set_stream(NULL) < 0);
    CHECK(flip_log_write(
        (flip_log_level_t)99,
        "TEST",
        "bad") < 0);
    CHECK(flip_log_write(
        FLIP_LOG_INFO,
        "",
        "bad") < 0);
    CHECK(flip_log_write(
        FLIP_LOG_INFO,
        "TEST",
        NULL) < 0);

    return failures
        ? EXIT_FAILURE
        : EXIT_SUCCESS;
}
