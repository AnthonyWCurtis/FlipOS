#define _GNU_SOURCE

#include "flip_gpu_kms.h"

#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

#define DRM_MODE_CONNECTED 1
#define DRM_DISPLAY_MODE_LEN 32

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
    char name[DRM_DISPLAY_MODE_LEN];
} drmModeModeInfo;

_Static_assert(
    sizeof(flip_gpu_kms_mode_t) == sizeof(drmModeModeInfo),
    "FlipOS KMS mode ABI must match libdrm");

typedef struct {
    int count_fbs;
    uint32_t *fbs;
    int count_crtcs;
    uint32_t *crtcs;
    int count_connectors;
    uint32_t *connectors;
    int count_encoders;
    uint32_t *encoders;
    uint32_t min_width;
    uint32_t max_width;
    uint32_t min_height;
    uint32_t max_height;
} drmModeRes;

typedef struct {
    uint32_t connector_id;
    uint32_t encoder_id;
    uint32_t connector_type;
    uint32_t connector_type_id;
    int connection;
    uint32_t mmWidth;
    uint32_t mmHeight;
    int subpixel;
    int count_modes;
    drmModeModeInfo *modes;
    int count_props;
    uint32_t *props;
    uint64_t *prop_values;
    int count_encoders;
    uint32_t *encoders;
} drmModeConnector;

typedef struct {
    uint32_t encoder_id;
    uint32_t encoder_type;
    uint32_t crtc_id;
    uint32_t possible_crtcs;
    uint32_t possible_clones;
} drmModeEncoder;

typedef drmModeRes *(*PFN_drmModeGetResources)(int fd);
typedef void (*PFN_drmModeFreeResources)(drmModeRes *resources);
typedef drmModeConnector *(*PFN_drmModeGetConnector)(
    int fd,
    uint32_t connector_id);
typedef void (*PFN_drmModeFreeConnector)(
    drmModeConnector *connector);
typedef drmModeEncoder *(*PFN_drmModeGetEncoder)(
    int fd,
    uint32_t encoder_id);
typedef void (*PFN_drmModeFreeEncoder)(
    drmModeEncoder *encoder);

static void *load_symbol(
    void *library,
    const char *name)
{
    void *symbol = dlsym(library, name);
    if (!symbol) {
        fprintf(
            stderr,
            "FLIP_GPU_KMS: missing symbol %s: %s\n",
            name,
            dlerror());
    }
    return symbol;
}

static uint32_t choose_crtc(
    int drm_fd,
    const drmModeRes *resources,
    const drmModeConnector *connector,
    PFN_drmModeGetEncoder get_encoder,
    PFN_drmModeFreeEncoder free_encoder)
{
    if (connector->encoder_id) {
        drmModeEncoder *encoder =
            get_encoder(
                drm_fd,
                connector->encoder_id);

        if (encoder) {
            uint32_t crtc_id =
                encoder->crtc_id;

            if (!crtc_id) {
                for (int index = 0;
                     index < resources->count_crtcs;
                     ++index) {

                    if (encoder->possible_crtcs &
                        (1u << index)) {

                        crtc_id =
                            resources->crtcs[index];
                        break;
                    }
                }
            }

            free_encoder(encoder);

            if (crtc_id)
                return crtc_id;
        }
    }

    for (int enc_index = 0;
         enc_index < connector->count_encoders;
         ++enc_index) {

        drmModeEncoder *encoder =
            get_encoder(
                drm_fd,
                connector->encoders[enc_index]);

        if (!encoder)
            continue;

        for (int crtc_index = 0;
             crtc_index < resources->count_crtcs;
             ++crtc_index) {

            if (encoder->possible_crtcs &
                (1u << crtc_index)) {

                uint32_t crtc_id =
                    resources->crtcs[crtc_index];

                free_encoder(encoder);
                return crtc_id;
            }
        }

        free_encoder(encoder);
    }

    return 0;
}

int flip_gpu_kms_discover(
    int drm_fd,
    flip_gpu_kms_info_t *info)
{
    if (drm_fd < 0 || !info)
        return -1;

    memset(
        info,
        0,
        sizeof(*info));

    void *libdrm =
        dlopen(
            "/usr/lib/libdrm.so.2",
            RTLD_NOW | RTLD_LOCAL);

    if (!libdrm) {
        fprintf(
            stderr,
            "FLIP_GPU_KMS: libdrm load failed: %s\n",
            dlerror());
        return -1;
    }

    PFN_drmModeGetResources get_resources =
        (PFN_drmModeGetResources)
        load_symbol(
            libdrm,
            "drmModeGetResources");

    PFN_drmModeFreeResources free_resources =
        (PFN_drmModeFreeResources)
        load_symbol(
            libdrm,
            "drmModeFreeResources");

    PFN_drmModeGetConnector get_connector =
        (PFN_drmModeGetConnector)
        load_symbol(
            libdrm,
            "drmModeGetConnector");

    PFN_drmModeFreeConnector free_connector =
        (PFN_drmModeFreeConnector)
        load_symbol(
            libdrm,
            "drmModeFreeConnector");

    PFN_drmModeGetEncoder get_encoder =
        (PFN_drmModeGetEncoder)
        load_symbol(
            libdrm,
            "drmModeGetEncoder");

    PFN_drmModeFreeEncoder free_encoder =
        (PFN_drmModeFreeEncoder)
        load_symbol(
            libdrm,
            "drmModeFreeEncoder");

    if (!get_resources ||
        !free_resources ||
        !get_connector ||
        !free_connector ||
        !get_encoder ||
        !free_encoder) {

        dlclose(libdrm);
        return -1;
    }

    drmModeRes *resources =
        get_resources(drm_fd);

    if (!resources) {
        fprintf(
            stderr,
            "FLIP_GPU_KMS: resource discovery failed\n");
        dlclose(libdrm);
        return -1;
    }

    int result = -1;

    for (int index = 0;
         index < resources->count_connectors;
         ++index) {

        drmModeConnector *connector =
            get_connector(
                drm_fd,
                resources->connectors[index]);

        if (!connector)
            continue;

        if (connector->connection ==
                DRM_MODE_CONNECTED &&
            connector->count_modes > 0) {

            uint32_t crtc_id =
                choose_crtc(
                    drm_fd,
                    resources,
                    connector,
                    get_encoder,
                    free_encoder);

            if (crtc_id) {
                const drmModeModeInfo *mode =
                    &connector->modes[0];

                info->connector_id =
                    connector->connector_id;

                info->crtc_id =
                    crtc_id;

                info->width =
                    mode->hdisplay;

                info->height =
                    mode->vdisplay;

                snprintf(
                    info->mode_name,
                    sizeof(info->mode_name),
                    "%s",
                    mode->name);

                memcpy(
                    &info->mode,
                    mode,
                    sizeof(info->mode));

                result = 0;
                free_connector(connector);
                break;
            }
        }

        free_connector(connector);
    }

    free_resources(resources);
    dlclose(libdrm);

    if (result == 0) {
        fprintf(
            stderr,
            "FLIP_GPU_KMS: DISCOVER_OK "
            "connector=%u crtc=%u mode=%ux%u name=%s\n",
            info->connector_id,
            info->crtc_id,
            info->width,
            info->height,
            info->mode_name);
    }

    return result;
}
