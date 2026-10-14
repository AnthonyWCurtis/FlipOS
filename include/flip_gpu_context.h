#ifndef FLIP_GPU_CONTEXT_H
#define FLIP_GPU_CONTEXT_H

#include "flip_gpu_kms.h"
#include "flip_gpu_protocol.h"

#include <stddef.h>
#include <stdint.h>

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

/* Wall-clock diagnostics only: CPU submission and swap/present wait; not GPU busy time. */
typedef struct {
    uint64_t draw_cpu_us;
    uint64_t present_us;
} flip_gpu_render_timing_t;

flip_gpu_context_t *flip_gpu_context_create(
    unsigned display_width,
    unsigned display_height,
    const flip_gpu_kms_info_t *scanout_kms);

void flip_gpu_context_destroy(
    flip_gpu_context_t *context);

const flip_gpu_context_info_t *flip_gpu_context_info(
    const flip_gpu_context_t *context);

/*
 * Upload a core frame, integer-scale/center it on the shared output surface,
 * and wait for GPU completion. Presentation to KMS is deliberately separate.
 */
int flip_gpu_context_set_overlay(
    flip_gpu_context_t *context,
    flip_gpu_overlay_slot_t slot,
    int x,
    int y,
    const void *data,
    unsigned width,
    unsigned height,
    size_t pitch);

int flip_gpu_context_clear_overlay(
    flip_gpu_context_t *context,
    flip_gpu_overlay_slot_t slot);

/* Retrieve timings from the most recent successfully rendered frame. */
int flip_gpu_context_last_render_timing(
    const flip_gpu_context_t *context,
    flip_gpu_render_timing_t *timing);

int flip_gpu_context_render(
    flip_gpu_context_t *context,
    const void *data,
    unsigned width,
    unsigned height,
    size_t pitch,
    flip_gpu_format_t format);

#endif
