#define _GNU_SOURCE

#include "flip_gpu_scanout.h"

#include <dlfcn.h>
#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GBM_FORMAT_XRGB8888 0x34325258u
#define GBM_BO_USE_SCANOUT (1u << 0)
#define GBM_BO_USE_RENDERING (1u << 2)
#define DRM_MODE_PAGE_FLIP_EVENT 0x01u
#define DRM_EVENT_CONTEXT_VERSION 2
#define FLIP_GPU_SCANOUT_FB_CACHE 8

struct gbm_surface;
struct gbm_bo;

typedef union {
    void *ptr;
    int32_t s32;
    uint32_t u32;
    uint64_t u64;
} gbm_bo_handle;

typedef struct {
    uint32_t crtc_id;
    uint32_t buffer_id;
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    int mode_valid;
    flip_gpu_kms_mode_t mode;
    int gamma_size;
} drmModeCrtc;

typedef struct {
    int version;
    void (*vblank_handler)(
        int fd,
        unsigned int sequence,
        unsigned int tv_sec,
        unsigned int tv_usec,
        void *user_data);
    void (*page_flip_handler)(
        int fd,
        unsigned int sequence,
        unsigned int tv_sec,
        unsigned int tv_usec,
        void *user_data);
    void (*page_flip_handler2)(
        int fd,
        unsigned int sequence,
        unsigned int tv_sec,
        unsigned int tv_usec,
        unsigned int crtc_id,
        void *user_data);
    void (*sequence_handler)(
        int fd,
        uint64_t sequence,
        uint64_t ns,
        void *user_data);
} drmEventContext;

typedef struct gbm_surface *(*PFN_gbm_surface_create)(
    struct gbm_device *gbm,
    uint32_t width,
    uint32_t height,
    uint32_t format,
    uint32_t flags);
typedef void (*PFN_gbm_surface_destroy)(
    struct gbm_surface *surface);
typedef struct gbm_bo *(*PFN_gbm_surface_lock_front_buffer)(
    struct gbm_surface *surface);
typedef void (*PFN_gbm_surface_release_buffer)(
    struct gbm_surface *surface,
    struct gbm_bo *bo);
typedef gbm_bo_handle (*PFN_gbm_bo_get_handle)(
    struct gbm_bo *bo);
typedef uint32_t (*PFN_gbm_bo_get_stride)(
    struct gbm_bo *bo);

typedef drmModeCrtc *(*PFN_drmModeGetCrtc)(
    int fd,
    uint32_t crtc_id);
typedef void (*PFN_drmModeFreeCrtc)(
    drmModeCrtc *crtc);
typedef int (*PFN_drmModeAddFB)(
    int fd,
    uint32_t width,
    uint32_t height,
    uint8_t depth,
    uint8_t bpp,
    uint32_t pitch,
    uint32_t bo_handle,
    uint32_t *buffer_id);
typedef int (*PFN_drmModeRmFB)(
    int fd,
    uint32_t buffer_id);
typedef int (*PFN_drmModeSetCrtc)(
    int fd,
    uint32_t crtc_id,
    uint32_t buffer_id,
    uint32_t x,
    uint32_t y,
    uint32_t *connectors,
    int count,
    const flip_gpu_kms_mode_t *mode);
typedef int (*PFN_drmModePageFlip)(
    int fd,
    uint32_t crtc_id,
    uint32_t buffer_id,
    uint32_t flags,
    void *user_data);
typedef int (*PFN_drmHandleEvent)(
    int fd,
    drmEventContext *context);

typedef struct {
    struct gbm_bo *bo;
    uint32_t fb_id;
} framebuffer_entry_t;

struct flip_gpu_scanout {
    void *libdrm;
    int drm_fd;
    flip_gpu_kms_info_t kms;
    drmModeCrtc *original_crtc;

    struct gbm_surface *surface;
    struct gbm_bo *front_bo;

    framebuffer_entry_t framebuffers[
        FLIP_GPU_SCANOUT_FB_CACHE];

    PFN_gbm_surface_destroy gbm_surface_destroy;
    PFN_gbm_surface_lock_front_buffer gbm_surface_lock_front_buffer;
    PFN_gbm_surface_release_buffer gbm_surface_release_buffer;
    PFN_gbm_bo_get_handle gbm_bo_get_handle;
    PFN_gbm_bo_get_stride gbm_bo_get_stride;

    PFN_drmModeFreeCrtc drmModeFreeCrtc;
    PFN_drmModeAddFB drmModeAddFB;
    PFN_drmModeRmFB drmModeRmFB;
    PFN_drmModeSetCrtc drmModeSetCrtc;
    PFN_drmModePageFlip drmModePageFlip;
    PFN_drmHandleEvent drmHandleEvent;

    int active;
};

static void *load_symbol(
    void *library,
    const char *name)
{
    void *symbol = dlsym(library, name);
    if (!symbol) {
        fprintf(
            stderr,
            "FLIP_GPU_SCANOUT: missing %s: %s\n",
            name,
            dlerror());
    }
    return symbol;
}

static uint32_t framebuffer_for_bo(
    flip_gpu_scanout_t *scanout,
    struct gbm_bo *bo)
{
    for (unsigned index = 0;
         index < FLIP_GPU_SCANOUT_FB_CACHE;
         ++index) {

        if (scanout->framebuffers[index].bo == bo)
            return scanout->framebuffers[index].fb_id;
    }

    unsigned free_index =
        FLIP_GPU_SCANOUT_FB_CACHE;

    for (unsigned index = 0;
         index < FLIP_GPU_SCANOUT_FB_CACHE;
         ++index) {

        if (!scanout->framebuffers[index].bo) {
            free_index = index;
            break;
        }
    }

    if (free_index ==
        FLIP_GPU_SCANOUT_FB_CACHE) {

        fprintf(
            stderr,
            "FLIP_GPU_SCANOUT: framebuffer cache exhausted\n");
        return 0;
    }

    gbm_bo_handle handle =
        scanout->gbm_bo_get_handle(bo);

    uint32_t stride =
        scanout->gbm_bo_get_stride(bo);

    uint32_t fb_id = 0;

    if (scanout->drmModeAddFB(
            scanout->drm_fd,
            scanout->kms.width,
            scanout->kms.height,
            24,
            32,
            stride,
            handle.u32,
            &fb_id) < 0) {

        fprintf(
            stderr,
            "FLIP_GPU_SCANOUT: drmModeAddFB failed: %s\n",
            strerror(errno));
        return 0;
    }

    scanout->framebuffers[free_index].bo = bo;
    scanout->framebuffers[free_index].fb_id = fb_id;

    return fb_id;
}

static void page_flip_handler(
    int fd,
    unsigned int sequence,
    unsigned int tv_sec,
    unsigned int tv_usec,
    void *user_data)
{
    (void)fd;
    (void)sequence;
    (void)tv_sec;
    (void)tv_usec;

    int *pending = user_data;
    if (pending)
        *pending = 0;
}

flip_gpu_scanout_t *flip_gpu_scanout_create(
    void *mali_library,
    int drm_fd,
    struct gbm_device *gbm,
    const flip_gpu_kms_info_t *kms)
{
    if (!mali_library ||
        drm_fd < 0 ||
        !gbm ||
        !kms ||
        kms->width == 0 ||
        kms->height == 0 ||
        kms->connector_id == 0 ||
        kms->crtc_id == 0) {

        return NULL;
    }

    flip_gpu_scanout_t *scanout =
        calloc(
            1,
            sizeof(*scanout));

    if (!scanout)
        return NULL;

    scanout->drm_fd = drm_fd;
    scanout->kms = *kms;

    PFN_gbm_surface_create gbm_surface_create =
        (PFN_gbm_surface_create)
        load_symbol(
            mali_library,
            "gbm_surface_create");

    scanout->gbm_surface_destroy =
        (PFN_gbm_surface_destroy)
        load_symbol(
            mali_library,
            "gbm_surface_destroy");

    scanout->gbm_surface_lock_front_buffer =
        (PFN_gbm_surface_lock_front_buffer)
        load_symbol(
            mali_library,
            "gbm_surface_lock_front_buffer");

    scanout->gbm_surface_release_buffer =
        (PFN_gbm_surface_release_buffer)
        load_symbol(
            mali_library,
            "gbm_surface_release_buffer");

    scanout->gbm_bo_get_handle =
        (PFN_gbm_bo_get_handle)
        load_symbol(
            mali_library,
            "gbm_bo_get_handle");

    scanout->gbm_bo_get_stride =
        (PFN_gbm_bo_get_stride)
        load_symbol(
            mali_library,
            "gbm_bo_get_stride");

    if (!gbm_surface_create ||
        !scanout->gbm_surface_destroy ||
        !scanout->gbm_surface_lock_front_buffer ||
        !scanout->gbm_surface_release_buffer ||
        !scanout->gbm_bo_get_handle ||
        !scanout->gbm_bo_get_stride) {

        goto fail;
    }

    scanout->libdrm =
        dlopen(
            "/usr/lib/libdrm.so.2",
            RTLD_NOW | RTLD_LOCAL);

    if (!scanout->libdrm) {
        fprintf(
            stderr,
            "FLIP_GPU_SCANOUT: libdrm load failed: %s\n",
            dlerror());
        goto fail;
    }

    PFN_drmModeGetCrtc drmModeGetCrtc =
        (PFN_drmModeGetCrtc)
        load_symbol(
            scanout->libdrm,
            "drmModeGetCrtc");

    scanout->drmModeFreeCrtc =
        (PFN_drmModeFreeCrtc)
        load_symbol(
            scanout->libdrm,
            "drmModeFreeCrtc");

    scanout->drmModeAddFB =
        (PFN_drmModeAddFB)
        load_symbol(
            scanout->libdrm,
            "drmModeAddFB");

    scanout->drmModeRmFB =
        (PFN_drmModeRmFB)
        load_symbol(
            scanout->libdrm,
            "drmModeRmFB");

    scanout->drmModeSetCrtc =
        (PFN_drmModeSetCrtc)
        load_symbol(
            scanout->libdrm,
            "drmModeSetCrtc");

    scanout->drmModePageFlip =
        (PFN_drmModePageFlip)
        load_symbol(
            scanout->libdrm,
            "drmModePageFlip");

    scanout->drmHandleEvent =
        (PFN_drmHandleEvent)
        load_symbol(
            scanout->libdrm,
            "drmHandleEvent");

    if (!drmModeGetCrtc ||
        !scanout->drmModeFreeCrtc ||
        !scanout->drmModeAddFB ||
        !scanout->drmModeRmFB ||
        !scanout->drmModeSetCrtc ||
        !scanout->drmModePageFlip ||
        !scanout->drmHandleEvent) {

        goto fail;
    }

    scanout->original_crtc =
        drmModeGetCrtc(
            drm_fd,
            kms->crtc_id);

    if (!scanout->original_crtc) {
        fprintf(
            stderr,
            "FLIP_GPU_SCANOUT: failed to capture original CRTC\n");
        goto fail;
    }

    scanout->surface =
        gbm_surface_create(
            gbm,
            kms->width,
            kms->height,
            GBM_FORMAT_XRGB8888,
            GBM_BO_USE_SCANOUT |
            GBM_BO_USE_RENDERING);

    if (!scanout->surface) {
        fprintf(
            stderr,
            "FLIP_GPU_SCANOUT: GBM surface creation failed\n");
        goto fail;
    }

    fprintf(
        stderr,
        "FLIP_GPU_SCANOUT: PREPARED "
        "connector=%u crtc=%u mode=%ux%u\n",
        kms->connector_id,
        kms->crtc_id,
        kms->width,
        kms->height);

    return scanout;

fail:
    flip_gpu_scanout_destroy(scanout);
    return NULL;
}

void *flip_gpu_scanout_native_window(
    flip_gpu_scanout_t *scanout)
{
    return scanout
        ? scanout->surface
        : NULL;
}

int flip_gpu_scanout_present(
    flip_gpu_scanout_t *scanout)
{
    if (!scanout ||
        !scanout->surface) {

        return -1;
    }

    struct gbm_bo *next_bo =
        scanout->gbm_surface_lock_front_buffer(
            scanout->surface);

    if (!next_bo) {
        fprintf(
            stderr,
            "FLIP_GPU_SCANOUT: lock front buffer failed\n");
        return -1;
    }

    uint32_t next_fb =
        framebuffer_for_bo(
            scanout,
            next_bo);

    if (!next_fb) {
        scanout->gbm_surface_release_buffer(
            scanout->surface,
            next_bo);
        return -1;
    }

    if (!scanout->active) {
        uint32_t connector =
            scanout->kms.connector_id;

        if (scanout->drmModeSetCrtc(
                scanout->drm_fd,
                scanout->kms.crtc_id,
                next_fb,
                0,
                0,
                &connector,
                1,
                &scanout->kms.mode) < 0) {

            fprintf(
                stderr,
                "FLIP_GPU_SCANOUT: initial set CRTC failed: %s\n",
                strerror(errno));

            scanout->gbm_surface_release_buffer(
                scanout->surface,
                next_bo);
            return -1;
        }

        scanout->front_bo =
            next_bo;

        scanout->active = 1;
        return 0;
    }

    int pending = 1;

    if (scanout->drmModePageFlip(
            scanout->drm_fd,
            scanout->kms.crtc_id,
            next_fb,
            DRM_MODE_PAGE_FLIP_EVENT,
            &pending) < 0) {

        fprintf(
            stderr,
            "FLIP_GPU_SCANOUT: page flip failed: %s\n",
            strerror(errno));

        scanout->gbm_surface_release_buffer(
            scanout->surface,
            next_bo);
        return -1;
    }

    drmEventContext event_context;
    memset(
        &event_context,
        0,
        sizeof(event_context));

    event_context.version =
        DRM_EVENT_CONTEXT_VERSION;

    event_context.page_flip_handler =
        page_flip_handler;

    while (pending) {
        struct pollfd poll_fd = {
            .fd = scanout->drm_fd,
            .events = POLLIN,
            .revents = 0
        };

        int poll_result =
            poll(
                &poll_fd,
                1,
                -1);

        if (poll_result < 0 &&
            errno == EINTR) {

            continue;
        }

        if (poll_result <= 0) {
            fprintf(
                stderr,
                "FLIP_GPU_SCANOUT: page flip poll failed: %s\n",
                strerror(errno));

            scanout->gbm_surface_release_buffer(
                scanout->surface,
                next_bo);
            return -1;
        }

        if (scanout->drmHandleEvent(
                scanout->drm_fd,
                &event_context) < 0) {

            fprintf(
                stderr,
                "FLIP_GPU_SCANOUT: drmHandleEvent failed: %s\n",
                strerror(errno));

            scanout->gbm_surface_release_buffer(
                scanout->surface,
                next_bo);
            return -1;
        }
    }

    if (scanout->front_bo) {
        scanout->gbm_surface_release_buffer(
            scanout->surface,
            scanout->front_bo);
    }

    scanout->front_bo =
        next_bo;

    return 0;
}

void flip_gpu_scanout_destroy(
    flip_gpu_scanout_t *scanout)
{
    if (!scanout)
        return;

    if (scanout->active &&
        scanout->original_crtc &&
        scanout->drmModeSetCrtc) {

        uint32_t connector =
            scanout->kms.connector_id;

        const flip_gpu_kms_mode_t *mode =
            scanout->original_crtc->mode_valid
            ? &scanout->original_crtc->mode
            : NULL;

        (void)scanout->drmModeSetCrtc(
            scanout->drm_fd,
            scanout->original_crtc->crtc_id,
            scanout->original_crtc->buffer_id,
            scanout->original_crtc->x,
            scanout->original_crtc->y,
            &connector,
            1,
            mode);
    }

    if (scanout->front_bo &&
        scanout->surface &&
        scanout->gbm_surface_release_buffer) {

        scanout->gbm_surface_release_buffer(
            scanout->surface,
            scanout->front_bo);

        scanout->front_bo = NULL;
    }

    if (scanout->drmModeRmFB) {
        for (unsigned index = 0;
             index < FLIP_GPU_SCANOUT_FB_CACHE;
             ++index) {

            if (scanout->framebuffers[index].fb_id) {
                (void)scanout->drmModeRmFB(
                    scanout->drm_fd,
                    scanout->framebuffers[index].fb_id);
            }
        }
    }

    if (scanout->surface &&
        scanout->gbm_surface_destroy) {

        scanout->gbm_surface_destroy(
            scanout->surface);
    }

    if (scanout->original_crtc &&
        scanout->drmModeFreeCrtc) {

        scanout->drmModeFreeCrtc(
            scanout->original_crtc);
    }

    if (scanout->libdrm)
        dlclose(scanout->libdrm);

    free(scanout);
}
