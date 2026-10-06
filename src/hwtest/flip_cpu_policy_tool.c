#include "flip_cpu_policy.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_khz(const char *text, unsigned *khz)
{
    if (!text || !khz)
        return -1;

    errno = 0;
    char *end = NULL;
    unsigned long value = strtoul(text, &end, 10);

    if (errno || end == text || *end != '\0' ||
        value == 0 || value > 0xffffffffUL)
        return -1;

    *khz = (unsigned)value;
    return 0;
}

static int print_status(void)
{
    flip_cpu_policy_info_t info;
    flip_cpu_policy_snapshot_t snapshot;

    if (flip_cpu_policy_query_info(&info) < 0 ||
        flip_cpu_policy_snapshot(&snapshot) < 0)
        return -1;

    printf("governor=%s\n", snapshot.governor);
    printf("min_khz=%u\n", snapshot.min_khz);
    printf("max_khz=%u\n", snapshot.max_khz);
    printf("available_khz=");

    for (size_t i = 0; i < info.frequency_count; ++i)
        printf("%s%u", i ? " " : "", info.frequencies_khz[i]);

    printf("\n");
    return 0;
}

static void usage(const char *program)
{
    fprintf(stderr,
            "usage: %s status | max KHZ | limits MIN_KHZ MAX_KHZ\n",
            program);
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "status") == 0)
        return print_status() == 0 ? EXIT_SUCCESS : EXIT_FAILURE;

    if (argc == 3 && strcmp(argv[1], "max") == 0) {
        unsigned max_khz = 0;

        if (parse_khz(argv[2], &max_khz) < 0 ||
            flip_cpu_policy_set_max(max_khz) < 0)
            return EXIT_FAILURE;

        return print_status() == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (argc == 4 && strcmp(argv[1], "limits") == 0) {
        unsigned min_khz = 0;
        unsigned max_khz = 0;

        if (parse_khz(argv[2], &min_khz) < 0 ||
            parse_khz(argv[3], &max_khz) < 0 ||
            flip_cpu_policy_set_limits(min_khz, max_khz) < 0)
            return EXIT_FAILURE;

        return print_status() == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    usage(argv[0]);
    return EXIT_FAILURE;
}
