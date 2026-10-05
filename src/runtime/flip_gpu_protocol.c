#include "flip_gpu_protocol.h"

#include <limits.h>

static unsigned bytes_per_pixel(flip_gpu_format_t format)
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
    unsigned bpp = bytes_per_pixel(format);

    if (!header || bpp == 0 || width == 0 || height == 0 ||
        width > FLIP_GPU_MAX_WIDTH || height > FLIP_GPU_MAX_HEIGHT ||
        pitch < (size_t)width * bpp || pitch > UINT32_MAX ||
        height > UINT32_MAX / pitch) {
        return -1;
    }

    size_t payload = pitch * height;
    if (payload > FLIP_GPU_MAX_PAYLOAD)
        return -1;

    header->magic = FLIP_GPU_PROTOCOL_MAGIC;
    header->version = FLIP_GPU_PROTOCOL_VERSION;
    header->format = (uint16_t)format;
    header->width = width;
    header->height = height;
    header->pitch = (uint32_t)pitch;
    header->payload_bytes = (uint32_t)payload;
    header->sequence = sequence;
    return 0;
}

int flip_gpu_frame_header_validate(
    const flip_gpu_frame_header_t *header)
{
    flip_gpu_frame_header_t expected;

    if (!header ||
        header->magic != FLIP_GPU_PROTOCOL_MAGIC ||
        header->version != FLIP_GPU_PROTOCOL_VERSION ||
        flip_gpu_frame_header_init(
            &expected,
            (flip_gpu_format_t)header->format,
            header->width,
            header->height,
            header->pitch,
            header->sequence) < 0) {
        return -1;
    }

    return expected.payload_bytes == header->payload_bytes ? 0 : -1;
}
