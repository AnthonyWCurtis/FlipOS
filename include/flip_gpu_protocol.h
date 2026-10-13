#ifndef FLIP_GPU_PROTOCOL_H
#define FLIP_GPU_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#define FLIP_GPU_SOCKET_PATH "/tmp/flipos-gpu.sock"

#define FLIP_GPU_PROTOCOL_VERSION 1u
#define FLIP_GPU_FRAME_MAGIC 0x46504756u
#define FLIP_GPU_OVERLAY_MAGIC 0x4650474fu

#define FLIP_GPU_MAX_WIDTH 1024u
#define FLIP_GPU_MAX_HEIGHT 1024u
#define FLIP_GPU_MAX_PAYLOAD (FLIP_GPU_MAX_WIDTH * FLIP_GPU_MAX_HEIGHT * 4u)

#define FLIP_GPU_OVERLAY_SLOTS 9u
#define FLIP_GPU_OVERLAY_MAX_PAYLOAD (128u * 1024u)

#define FLIP_GPU_OVERLAY_ACTIVE 0x0001u

typedef enum {
    FLIP_GPU_FORMAT_RGB565 = 1,
    FLIP_GPU_FORMAT_XRGB8888 = 2
} flip_gpu_format_t;

typedef enum {
    FLIP_GPU_OVERLAY_OSD = 0,
    FLIP_GPU_OVERLAY_PERF = 1,
    FLIP_GPU_OVERLAY_MENU_0 = 2,
    FLIP_GPU_OVERLAY_MENU_1 = 3,
    FLIP_GPU_OVERLAY_MENU_2 = 4,
    FLIP_GPU_OVERLAY_MENU_3 = 5,
    FLIP_GPU_OVERLAY_MENU_4 = 6,
    FLIP_GPU_OVERLAY_MENU_5 = 7,
    FLIP_GPU_OVERLAY_MENU_6 = 8
} flip_gpu_overlay_slot_t;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t format;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t payload_bytes;
    uint64_t sequence;
} flip_gpu_frame_header_t;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t slot;
    uint16_t format;
    uint16_t flags;
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t payload_bytes;
} flip_gpu_overlay_header_t;

int flip_gpu_frame_header_init(
    flip_gpu_frame_header_t *header,
    flip_gpu_format_t format,
    unsigned width,
    unsigned height,
    size_t pitch,
    uint64_t sequence);

int flip_gpu_frame_header_validate(
    const flip_gpu_frame_header_t *header);

int flip_gpu_overlay_header_init(
    flip_gpu_overlay_header_t *header,
    flip_gpu_overlay_slot_t slot,
    int x,
    int y,
    unsigned width,
    unsigned height,
    size_t pitch);

int flip_gpu_overlay_header_clear(
    flip_gpu_overlay_header_t *header,
    flip_gpu_overlay_slot_t slot);

int flip_gpu_overlay_header_validate(
    const flip_gpu_overlay_header_t *header);

#endif
