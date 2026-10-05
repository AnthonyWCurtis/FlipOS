#define _POSIX_C_SOURCE 200809L

#include "flip_gpu_client.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static void sleep_ns(long nanoseconds)
{
    struct timespec delay = { .tv_sec = 0, .tv_nsec = nanoseconds };
    while (nanosleep(&delay, &delay) < 0)
        ;
}

int main(int argc, char **argv)
{
    unsigned frames = 300;
    if (argc > 1)
        frames = (unsigned)strtoul(argv[1], NULL, 10);

    const unsigned width = 240;
    const unsigned height = 160;
    const size_t pitch = width * sizeof(uint16_t);
    uint16_t *pixels = malloc(pitch * height);
    if (!pixels)
        return 1;

    flip_gpu_client_t client;
    if (flip_gpu_client_open(&client) < 0) {
        fprintf(stderr, "FLIP_GPU_SENDER: connect failed\n");
        free(pixels);
        return 1;
    }

    for (unsigned frame = 0; frame < frames; ++frame) {
        for (unsigned y = 0; y < height; ++y) {
            for (unsigned x = 0; x < width; ++x) {
                unsigned r = (x + frame) & 31u;
                unsigned g = (y + frame) & 63u;
                unsigned b = (x + y + frame) & 31u;
                pixels[y * width + x] = (uint16_t)((r << 11) | (g << 5) | b);
            }
        }

        int submit = flip_gpu_client_submit(
            &client,
            pixels,
            width,
            height,
            pitch,
            FLIP_GPU_FORMAT_RGB565);
        if (submit < 0) {
            fprintf(stderr, "FLIP_GPU_SENDER: submit failed frame=%u\n", frame);
            flip_gpu_client_close(&client);
            free(pixels);
            return 1;
        }

        sleep_ns(16742706L);
    }

    fprintf(stderr,
        "FLIP_GPU_SENDER: submitted=%llu dropped=%llu\n",
        (unsigned long long)client.submitted,
        (unsigned long long)client.dropped);

    flip_gpu_client_close(&client);
    free(pixels);
    return 0;
}
