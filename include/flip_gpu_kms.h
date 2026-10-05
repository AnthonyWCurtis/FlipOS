#ifndef FLIP_GPU_KMS_H
#define FLIP_GPU_KMS_H

#include <stdint.h>

#define FLIP_GPU_KMS_MODE_NAME_LENGTH 32

typedef struct {
    uint32_t clock;
    uint16_t hdisplay;
    uint16_t hsync_start;
    uint16_t hsync_end;
    uint16_t htotal;
    uint16_t hskew;
    uint16_t vdisplay;
    uint16_t vsync_start;
    uint16_t vsync_end;
    uint16_t vtotal;
    uint16_t vscan;
    uint32_t vrefresh;
    uint32_t flags;
    uint32_t type;
    char name[FLIP_GPU_KMS_MODE_NAME_LENGTH];
} flip_gpu_kms_mode_t;

typedef struct {
    uint32_t connector_id;
    uint32_t crtc_id;
    unsigned width;
    unsigned height;
    char mode_name[FLIP_GPU_KMS_MODE_NAME_LENGTH];
    flip_gpu_kms_mode_t mode;
} flip_gpu_kms_info_t;

/*
 * Read-only KMS discovery. This never changes the active CRTC or framebuffer.
 */
int flip_gpu_kms_discover(
    int drm_fd,
    flip_gpu_kms_info_t *info);

#endif
