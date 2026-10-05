#include "flip_gpu_protocol.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    flip_gpu_frame_header_t header;

    assert(flip_gpu_frame_header_init(
        &header,
        FLIP_GPU_FORMAT_RGB565,
        240,
        160,
        512,
        7) == 0);
    assert(header.payload_bytes == 81920);
    assert(header.sequence == 7);
    assert(flip_gpu_frame_header_validate(&header) == 0);

    assert(flip_gpu_frame_header_init(
        &header,
        FLIP_GPU_FORMAT_RGB565,
        160,
        144,
        320,
        1) == 0);
    assert(header.payload_bytes == 46080);

    assert(flip_gpu_frame_header_init(
        &header,
        FLIP_GPU_FORMAT_XRGB8888,
        240,
        160,
        960,
        2) == 0);
    assert(header.payload_bytes == 153600);

    assert(flip_gpu_frame_header_init(
        &header,
        FLIP_GPU_FORMAT_RGB565,
        240,
        160,
        100,
        0) < 0);

    assert(flip_gpu_frame_header_init(
        &header,
        (flip_gpu_format_t)99,
        240,
        160,
        480,
        0) < 0);

    assert(flip_gpu_frame_header_init(
        &header,
        FLIP_GPU_FORMAT_RGB565,
        FLIP_GPU_MAX_WIDTH + 1,
        160,
        (FLIP_GPU_MAX_WIDTH + 1) * 2,
        0) < 0);

    assert(flip_gpu_frame_header_init(
        &header,
        FLIP_GPU_FORMAT_RGB565,
        240,
        160,
        480,
        3) == 0);
    header.magic = 0;
    assert(flip_gpu_frame_header_validate(&header) < 0);

    puts("flip_gpu_protocol tests passed");
    return 0;
}
