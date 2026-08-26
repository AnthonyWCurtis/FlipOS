#define _GNU_SOURCE

#include "flip_drm.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#define DRM_DEVICE "/dev/dri/card0"

/*
 * Confirmed on our Miyoo Flip stock runtime.
 */
#define FLIP_CONNECTOR_ID 179
#define FLIP_CRTC_ID      107

#define DRM_IOCTL_BASE 'd'
#define DRM_IOWR(nr, type) _IOWR(DRM_IOCTL_BASE, nr, type)

#define DRM_IOCTL_MODE_GETCRTC \
    DRM_IOWR(0xA1, struct drm_mode_crtc)

#define DRM_IOCTL_MODE_SETCRTC \
    DRM_IOWR(0xA2, struct drm_mode_crtc)

#define DRM_IOCTL_MODE_CREATE_DUMB \
    DRM_IOWR(0xB2, struct drm_mode_create_dumb)

#define DRM_IOCTL_MODE_MAP_DUMB \
    DRM_IOWR(0xB3, struct drm_mode_map_dumb)

#define DRM_IOCTL_MODE_DESTROY_DUMB \
    DRM_IOWR(0xB4, struct drm_mode_destroy_dumb)

#define DRM_IOCTL_MODE_ADDFB \
    DRM_IOWR(0xAE, struct drm_mode_fb_cmd)

#define DRM_IOCTL_MODE_RMFB \
    DRM_IOWR(0xAF, uint32_t)

#define DRM_IOCTL_MODE_PAGE_FLIP \
    DRM_IOWR(0xB0, struct drm_mode_crtc_page_flip)

#define DRM_MODE_PAGE_FLIP_EVENT 0x01

struct drm_mode_modeinfo {
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

    char name[32];
};

struct drm_mode_crtc {
    uint64_t set_connectors_ptr;

    uint32_t count_connectors;
    uint32_t crtc_id;
    uint32_t fb_id;

    uint32_t x;
    uint32_t y;

    uint32_t gamma_size;
    uint32_t mode_valid;

    struct drm_mode_modeinfo mode;
};

struct drm_mode_create_dumb {
    uint32_t height;
    uint32_t width;
    uint32_t bpp;
    uint32_t flags;

    uint32_t handle;
    uint32_t pitch;

    uint64_t size;
};

struct drm_mode_map_dumb {
    uint32_t handle;
    uint32_t pad;
    uint64_t offset;
};

struct drm_mode_destroy_dumb {
    uint32_t handle;
};

struct drm_mode_fb_cmd {
    uint32_t fb_id;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
    uint32_t depth;
    uint32_t handle;
};

struct drm_mode_crtc_page_flip {
    uint32_t crtc_id;
    uint32_t fb_id;
    uint32_t flags;
    uint32_t reserved;
    uint64_t user_data;
};

static uint64_t monotonic_us(void)
{
    struct timespec now;

    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0)
        return 0;

    return
        (uint64_t)now.tv_sec * 1000000ULL +
        (uint64_t)now.tv_nsec / 1000ULL;
}

static uint64_t elapsed_us(uint64_t start_us)
{
    uint64_t now_us = monotonic_us();

    if (now_us < start_us)
        return 0;

    return now_us - start_us;
}

static void update_aliases(flip_drm_t *drm)
{
    drm->fb_id =
        drm->fb_ids[drm->front_index];

    drm->handle =
        drm->handles[drm->front_index];

    drm->pitch =
        drm->pitches[drm->back_index];

    drm->size =
        drm->sizes[drm->back_index];

    drm->pixels =
        drm->maps[drm->back_index];
}

static int create_buffer(
    flip_drm_t *drm,
    int index)
{
    struct drm_mode_create_dumb create;
    memset(&create, 0, sizeof(create));

    create.width =
        drm->width;

    create.height =
        drm->height;

    create.bpp = 32;

    if (ioctl(
            drm->fd,
            DRM_IOCTL_MODE_CREATE_DUMB,
            &create) < 0) {

        fprintf(
            stderr,
            "flip_drm: CREATE_DUMB[%d] failed: %s\n",
            index,
            strerror(errno));

        return -1;
    }

    drm->handles[index] =
        create.handle;

    drm->pitches[index] =
        create.pitch;

    drm->sizes[index] =
        create.size;

    struct drm_mode_fb_cmd fb;
    memset(&fb, 0, sizeof(fb));

    fb.width =
        drm->width;

    fb.height =
        drm->height;

    fb.pitch =
        create.pitch;

    fb.bpp = 32;
    fb.depth = 24;

    fb.handle =
        create.handle;

    if (ioctl(
            drm->fd,
            DRM_IOCTL_MODE_ADDFB,
            &fb) < 0) {

        fprintf(
            stderr,
            "flip_drm: ADDFB[%d] failed: %s\n",
            index,
            strerror(errno));

        return -1;
    }

    drm->fb_ids[index] =
        fb.fb_id;

    struct drm_mode_map_dumb map;
    memset(&map, 0, sizeof(map));

    map.handle =
        create.handle;

    if (ioctl(
            drm->fd,
            DRM_IOCTL_MODE_MAP_DUMB,
            &map) < 0) {

        fprintf(
            stderr,
            "flip_drm: MAP_DUMB[%d] failed: %s\n",
            index,
            strerror(errno));

        return -1;
    }

    void *ptr =
        mmap(
            NULL,
            create.size,
            PROT_READ | PROT_WRITE,
            MAP_SHARED,
            drm->fd,
            map.offset);

    if (ptr == MAP_FAILED) {

        fprintf(
            stderr,
            "flip_drm: mmap[%d] failed: %s\n",
            index,
            strerror(errno));

        return -1;
    }

    drm->maps[index] =
        (uint32_t *)ptr;

    memset(
        drm->maps[index],
        0,
        (size_t)drm->sizes[index]);

    fprintf(
        stderr,
        "flip_drm: buffer[%d] handle=%u fb=%u "
        "pitch=%u size=%llu\n",
        index,
        drm->handles[index],
        drm->fb_ids[index],
        drm->pitches[index],
        (unsigned long long)drm->sizes[index]);

    return 0;
}

int flip_drm_open(flip_drm_t *drm)
{
    memset(
        drm,
        0,
        sizeof(*drm));

    drm->fd = -1;

    drm->front_index = 0;
    drm->back_index = 1;

    drm->fd =
        open(
            DRM_DEVICE,
            O_RDWR);

    if (drm->fd < 0) {

        fprintf(
            stderr,
            "flip_drm: open %s failed: %s\n",
            DRM_DEVICE,
            strerror(errno));

        return -1;
    }

    drm->connector_id =
        FLIP_CONNECTOR_ID;

    drm->crtc_id =
        FLIP_CRTC_ID;

    /*
     * Inherit the mode already configured by stock firmware.
     */
    struct drm_mode_crtc current;
    memset(
        &current,
        0,
        sizeof(current));

    current.crtc_id =
        drm->crtc_id;

    if (ioctl(
            drm->fd,
            DRM_IOCTL_MODE_GETCRTC,
            &current) < 0) {

        fprintf(
            stderr,
            "flip_drm: GETCRTC id=%u failed: %s\n",
            drm->crtc_id,
            strerror(errno));

        flip_drm_close(drm);
        return -1;
    }

    if (!current.mode_valid) {

        fprintf(
            stderr,
            "flip_drm: CRTC %u has no valid active mode\n",
            drm->crtc_id);

        flip_drm_close(drm);
        return -1;
    }

    drm->width =
        current.mode.hdisplay;

    drm->height =
        current.mode.vdisplay;

    fprintf(
        stderr,
        "flip_drm: current connector=%u crtc=%u "
        "mode=%ux%u name=%s old_fb=%u\n",
        drm->connector_id,
        drm->crtc_id,
        drm->width,
        drm->height,
        current.mode.name,
        current.fb_id);

    if (drm->width != 640 ||
        drm->height != 480) {

        fprintf(
            stderr,
            "flip_drm: unexpected active mode %ux%u\n",
            drm->width,
            drm->height);

        flip_drm_close(drm);
        return -1;
    }

    if (create_buffer(
            drm,
            0) < 0) {

        flip_drm_close(drm);
        return -1;
    }

    if (create_buffer(
            drm,
            1) < 0) {

        flip_drm_close(drm);
        return -1;
    }

    /*
     * Put buffer 0 on-screen initially.
     * Applications immediately draw into hidden buffer 1.
     */
    uint32_t connector =
        drm->connector_id;

    struct drm_mode_crtc set;
    memset(
        &set,
        0,
        sizeof(set));

    set.set_connectors_ptr =
        (uint64_t)(uintptr_t)&connector;

    set.count_connectors = 1;

    set.crtc_id =
        drm->crtc_id;

    set.fb_id =
        drm->fb_ids[0];

    set.x = 0;
    set.y = 0;

    set.mode_valid = 1;
    set.mode = current.mode;

    if (ioctl(
            drm->fd,
            DRM_IOCTL_MODE_SETCRTC,
            &set) < 0) {

        fprintf(
            stderr,
            "flip_drm: SETCRTC connector=%u "
            "crtc=%u fb=%u failed: %s\n",
            drm->connector_id,
            drm->crtc_id,
            drm->fb_ids[0],
            strerror(errno));

        flip_drm_close(drm);
        return -1;
    }

    drm->front_index = 0;
    drm->back_index = 1;

    update_aliases(drm);

    fprintf(
        stderr,
        "flip_drm: DOUBLE_BUFFER_OK "
        "front_fb=%u back_fb=%u\n",
        drm->fb_ids[drm->front_index],
        drm->fb_ids[drm->back_index]);

    return 0;
}

int flip_drm_present(flip_drm_t *drm)
{
    if (!drm ||
        drm->fd < 0 ||
        !drm->pixels)
        return -1;

    drm->last_present_us = 0;
    drm->last_flip_ioctl_us = 0;
    drm->last_flip_wait_us = 0;
    drm->last_flip_read_us = 0;

    uint64_t present_start_us =
        monotonic_us();

    struct drm_mode_crtc_page_flip flip;
    memset(
        &flip,
        0,
        sizeof(flip));

    flip.crtc_id =
        drm->crtc_id;

    flip.fb_id =
        drm->fb_ids[drm->back_index];

    flip.flags =
        DRM_MODE_PAGE_FLIP_EVENT;

    flip.user_data = 1;

    uint64_t ioctl_start_us =
        monotonic_us();

    if (ioctl(
            drm->fd,
            DRM_IOCTL_MODE_PAGE_FLIP,
            &flip) < 0) {

        drm->last_flip_ioctl_us =
            elapsed_us(ioctl_start_us);
        drm->last_present_us =
            elapsed_us(present_start_us);

        fprintf(
            stderr,
            "flip_drm: PAGE_FLIP fb=%u failed: %s\n",
            flip.fb_id,
            strerror(errno));

        return -1;
    }

    drm->last_flip_ioctl_us =
        elapsed_us(ioctl_start_us);

    /*
     * Wait for the kernel's flip-complete event.
     * This is our vblank synchronization point.
     */
    struct pollfd pfd;

    pfd.fd =
        drm->fd;

    pfd.events =
        POLLIN;

    pfd.revents = 0;

    uint64_t wait_start_us =
        monotonic_us();

    while (1) {

        int rc =
            poll(
                &pfd,
                1,
                -1);

        if (rc > 0)
            break;

        if (rc < 0 &&
            errno == EINTR)
            continue;

        drm->last_flip_wait_us =
            elapsed_us(wait_start_us);
        drm->last_present_us =
            elapsed_us(present_start_us);

        fprintf(
            stderr,
            "flip_drm: page-flip poll failed: %s\n",
            strerror(errno));

        return -1;
    }

    drm->last_flip_wait_us =
        elapsed_us(wait_start_us);

    /*
     * Consume the DRM event. We currently only ever have one
     * outstanding page flip.
     */
    unsigned char event_buf[256];

    uint64_t read_start_us =
        monotonic_us();

    ssize_t got =
        read(
            drm->fd,
            event_buf,
            sizeof(event_buf));

    drm->last_flip_read_us =
        elapsed_us(read_start_us);

    if (got <= 0) {

        drm->last_present_us =
            elapsed_us(present_start_us);

        fprintf(
            stderr,
            "flip_drm: page-flip event read failed: %s\n",
            strerror(errno));

        return -1;
    }

    /*
     * The old back buffer is now on screen.
     * The old front buffer becomes writable.
     */
    int old_front =
        drm->front_index;

    drm->front_index =
        drm->back_index;

    drm->back_index =
        old_front;

    update_aliases(drm);

    drm->last_present_us =
        elapsed_us(present_start_us);

    return 0;
}

void flip_drm_close(flip_drm_t *drm)
{
    if (!drm)
        return;

    for (int i = 0; i < 2; ++i) {

        if (drm->maps[i]) {

            munmap(
                drm->maps[i],
                drm->sizes[i]);

            drm->maps[i] = NULL;
        }
    }

    if (drm->fd >= 0) {

        for (int i = 0; i < 2; ++i) {

            if (drm->fb_ids[i]) {

                uint32_t fb =
                    drm->fb_ids[i];

                ioctl(
                    drm->fd,
                    DRM_IOCTL_MODE_RMFB,
                    &fb);

                drm->fb_ids[i] = 0;
            }
        }

        for (int i = 0; i < 2; ++i) {

            if (drm->handles[i]) {

                struct drm_mode_destroy_dumb destroy;

                memset(
                    &destroy,
                    0,
                    sizeof(destroy));

                destroy.handle =
                    drm->handles[i];

                ioctl(
                    drm->fd,
                    DRM_IOCTL_MODE_DESTROY_DUMB,
                    &destroy);

                drm->handles[i] = 0;
            }
        }

        close(drm->fd);
        drm->fd = -1;
    }

    drm->pixels = NULL;
}
