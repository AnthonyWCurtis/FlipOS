#include "flip_gpu_protocol.h"

#include <limits.h>
#include <string.h>

static unsigned bytes_per_pixel(
    flip_gpu_format_t format)
{
    switch (format) {
        case FLIP_GPU_FORMAT_RGB565:
            return 2;
        case FLIP_GPU_FORMAT_XRGB8888:
            return 4;
        default:
            return 0;
    }
}

int flip_gpu_frame_header_init(
    flip_gpu_frame_header_t *header,
    flip_gpu_format_t format,
    unsigned width,
    unsigned height,
    size_t pitch,
    uint64_t sequence)
{
    unsigned bpp =
        bytes_per_pixel(format);

    if (!header ||
        bpp == 0 ||
        width == 0 ||
        height == 0 ||
        width > FLIP_GPU_MAX_WIDTH ||
        height > FLIP_GPU_MAX_HEIGHT ||
        pitch < (size_t)width * bpp ||
        pitch > UINT32_MAX ||
        height > UINT32_MAX / pitch) {

        return -1;
    }

    size_t payload =
        pitch * height;

    if (payload >
        FLIP_GPU_MAX_PAYLOAD) {

        return -1;
    }

    header->magic =
        FLIP_GPU_FRAME_MAGIC;
    header->version =
        FLIP_GPU_PROTOCOL_VERSION;
    header->format =
        (uint16_t)format;
    header->width = width;
    header->height = height;
    header->pitch = (uint32_t)pitch;
    header->payload_bytes =
        (uint32_t)payload;
    header->sequence = sequence;

    return 0;
}

int flip_gpu_frame_header_validate(
    const flip_gpu_frame_header_t *header)
{
    flip_gpu_frame_header_t expected;

    if (!header ||
        header->magic !=
            FLIP_GPU_FRAME_MAGIC ||
        header->version !=
            FLIP_GPU_PROTOCOL_VERSION ||
        flip_gpu_frame_header_init(
            &expected,
            (flip_gpu_format_t)header->format,
            header->width,
            header->height,
            header->pitch,
            header->sequence) < 0) {

        return -1;
    }

    return
        expected.payload_bytes ==
            header->payload_bytes
        ? 0
        : -1;
}

int flip_gpu_overlay_header_init(
    flip_gpu_overlay_header_t *header,
    flip_gpu_overlay_slot_t slot,
    int x,
    int y,
    unsigned width,
    unsigned height,
    size_t pitch)
{
    if (!header ||
        (unsigned)slot >=
            FLIP_GPU_OVERLAY_SLOTS ||
        x < 0 ||
        y < 0 ||
        width == 0 ||
        height == 0 ||
        width > FLIP_GPU_MAX_WIDTH ||
        height > FLIP_GPU_MAX_HEIGHT ||
        pitch < (size_t)width * 4u ||
        pitch > UINT32_MAX ||
        height > UINT32_MAX / pitch) {

        return -1;
    }

    size_t payload =
        pitch * height;

    if (payload >
        FLIP_GPU_OVERLAY_MAX_PAYLOAD) {

        return -1;
    }

    memset(
        header,
        0,
        sizeof(*header));

    header->magic =
        FLIP_GPU_OVERLAY_MAGIC;
    header->version =
        FLIP_GPU_PROTOCOL_VERSION;
    header->slot =
        (uint16_t)slot;
    header->format =
        FLIP_GPU_FORMAT_XRGB8888;
    header->flags =
        FLIP_GPU_OVERLAY_ACTIVE;
    header->x = x;
    header->y = y;
    header->width = width;
    header->height = height;
    header->pitch = (uint32_t)pitch;
    header->payload_bytes =
        (uint32_t)payload;

    return 0;
}

int flip_gpu_overlay_header_clear(
    flip_gpu_overlay_header_t *header,
    flip_gpu_overlay_slot_t slot)
{
    if (!header ||
        (unsigned)slot >=
            FLIP_GPU_OVERLAY_SLOTS) {

        return -1;
    }

    memset(
        header,
        0,
        sizeof(*header));

    header->magic =
        FLIP_GPU_OVERLAY_MAGIC;
    header->version =
        FLIP_GPU_PROTOCOL_VERSION;
    header->slot =
        (uint16_t)slot;
    header->format =
        FLIP_GPU_FORMAT_XRGB8888;

    return 0;
}

int flip_gpu_overlay_header_validate(
    const flip_gpu_overlay_header_t *header)
{
    if (!header ||
        header->magic !=
            FLIP_GPU_OVERLAY_MAGIC ||
        header->version !=
            FLIP_GPU_PROTOCOL_VERSION ||
        header->slot >=
            FLIP_GPU_OVERLAY_SLOTS ||
        header->format !=
            FLIP_GPU_FORMAT_XRGB8888) {

        return -1;
    }

    if (!(header->flags &
          FLIP_GPU_OVERLAY_ACTIVE)) {

        return
            header->payload_bytes == 0
            ? 0
            : -1;
    }

    flip_gpu_overlay_header_t expected;

    if (flip_gpu_overlay_header_init(
            &expected,
            (flip_gpu_overlay_slot_t)
                header->slot,
            header->x,
            header->y,
            header->width,
            header->height,
            header->pitch) < 0) {

        return -1;
    }

    return
        expected.payload_bytes ==
            header->payload_bytes
        ? 0
        : -1;
}
