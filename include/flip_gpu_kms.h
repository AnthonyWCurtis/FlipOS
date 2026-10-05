#ifndef FLIP_GPU_KMS_H
#define FLIP_GPU_KMS_H

#include <stdint.h>

typedef struct {
    uint32_t connector_id;
    uint32_t crtc_id;
    unsigned width;
    unsigned height;
    char mode_name[32];
} flip_gpu_kms_info_t;

/*
 * Read-only KMS discovery. This never changes the active CRTC or framebuffer.
 */
int flip_gpu_kms_discover(
    int drm_fd,
    flip_gpu_kms_info_t *info);

#endif
