#ifndef FLIP_DRM_H
#define FLIP_DRM_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    int fd;

    uint32_t connector_id;
    uint32_t crtc_id;

    uint32_t width;
    uint32_t height;

    /*
     * Compatibility aliases used by existing FlipOS code.
     * pitch/size are identical for both dumb buffers.
     * pixels always points at the BACK buffer which clients
     * should draw into.
     */
    uint32_t fb_id;
    uint32_t handle;
    uint32_t pitch;
    uint64_t size;
    uint32_t *pixels;

    /*
     * Double-buffered KMS state.
     */
    uint32_t fb_ids[2];
    uint32_t handles[2];
    uint32_t pitches[2];
    uint64_t sizes[2];
    uint32_t *maps[2];

    int front_index;
    int back_index;

} flip_drm_t;

int flip_drm_open(flip_drm_t *drm);
void flip_drm_close(flip_drm_t *drm);

/*
 * Present the completed back buffer at the next KMS page flip,
 * wait for the flip-complete event, then expose the old front
 * buffer as the next writable back buffer.
 */
int flip_drm_present(flip_drm_t *drm);

#endif
