#ifndef FLIP_GPU_CONTEXT_H
#define FLIP_GPU_CONTEXT_H

#include "flip_gpu_protocol.h"

#include <stddef.h>

typedef struct flip_gpu_context flip_gpu_context_t;

typedef struct {
    int egl_major;
    int egl_minor;
    const char *egl_vendor;
    const char *egl_version;
    const char *gl_vendor;
    const char *gl_renderer;
    const char *gl_version;
} flip_gpu_context_info_t;

flip_gpu_context_t *flip_gpu_context_create(
    unsigned display_width,
    unsigned display_height);

void flip_gpu_context_destroy(
    flip_gpu_context_t *context);

const flip_gpu_context_info_t *flip_gpu_context_info(
    const flip_gpu_context_t *context);

/*
 * Upload a core frame, integer-scale/center it on the shared output surface,
 * and wait for GPU completion. Presentation to KMS is deliberately separate.
 */
int flip_gpu_context_render(
    flip_gpu_context_t *context,
    const void *data,
    unsigned width,
    unsigned height,
    size_t pitch,
    flip_gpu_format_t format);

#endif
