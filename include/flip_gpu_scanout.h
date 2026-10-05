#ifndef FLIP_GPU_SCANOUT_H
#define FLIP_GPU_SCANOUT_H

#include "flip_gpu_kms.h"

struct gbm_device;

typedef struct flip_gpu_scanout flip_gpu_scanout_t;

/*
 * Prepare GBM/KMS scanout state without changing the active display.
 * The returned native window can be passed to eglCreateWindowSurface().
 */
flip_gpu_scanout_t *flip_gpu_scanout_create(
    void *mali_library,
    int drm_fd,
    struct gbm_device *gbm,
    const flip_gpu_kms_info_t *kms);

void *flip_gpu_scanout_native_window(
    flip_gpu_scanout_t *scanout);

/*
 * Present the most recently swapped GBM front buffer.
 * This is the first operation that changes the active CRTC/page flip.
 */
int flip_gpu_scanout_present(
    flip_gpu_scanout_t *scanout);

void flip_gpu_scanout_destroy(
    flip_gpu_scanout_t *scanout);

#endif
