#ifndef FLIP_GPU_CLIENT_H
#define FLIP_GPU_CLIENT_H

#include "flip_gpu_protocol.h"

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

typedef struct {
    int fd;
    pid_t service_pid;
    uint64_t sequence;
    uint64_t submitted;
    uint64_t dropped;
    uint64_t overlay_submitted;
    uint64_t overlay_dropped;
} flip_gpu_client_t;

int flip_gpu_client_open(
    flip_gpu_client_t *client);

int flip_gpu_client_launch(
    flip_gpu_client_t *client,
    const char *service_path,
    int scanout);

void flip_gpu_client_close(
    flip_gpu_client_t *client);

int flip_gpu_client_submit(
    flip_gpu_client_t *client,
    const void *data,
    unsigned width,
    unsigned height,
    size_t pitch,
    flip_gpu_format_t format);

int flip_gpu_client_submit_overlay(
    flip_gpu_client_t *client,
    flip_gpu_overlay_slot_t slot,
    int x,
    int y,
    const void *data,
    unsigned width,
    unsigned height,
    size_t pitch);

int flip_gpu_client_clear_overlay(
    flip_gpu_client_t *client,
    flip_gpu_overlay_slot_t slot);

#endif
