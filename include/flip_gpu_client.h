#ifndef FLIP_GPU_CLIENT_H
#define FLIP_GPU_CLIENT_H

#include "flip_gpu_protocol.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
    int fd;
    uint64_t sequence;
    uint64_t submitted;
    uint64_t dropped;
} flip_gpu_client_t;

int flip_gpu_client_open(flip_gpu_client_t *client);
void flip_gpu_client_close(flip_gpu_client_t *client);

/*
 * Returns:
 *   0 frame submitted
 *   1 frame intentionally dropped because the renderer queue is full
 *  -1 protocol/socket failure
 */
int flip_gpu_client_submit(
    flip_gpu_client_t *client,
    const void *data,
    unsigned width,
    unsigned height,
    size_t pitch,
    flip_gpu_format_t format);

#endif
