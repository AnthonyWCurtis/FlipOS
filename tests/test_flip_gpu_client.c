#define _POSIX_C_SOURCE 200809L

#include "flip_gpu_client.h"
#include "flip_gpu_protocol.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#ifndef TEST_GPU_SERVICE_PATH
#define TEST_GPU_SERVICE_PATH "build/test-flip-gpu-fake-service"
#endif

int main(void)
{
    unlink(FLIP_GPU_SOCKET_PATH);

    flip_gpu_client_t missing;
    assert(flip_gpu_client_open(&missing) < 0);

    int server = socket(AF_UNIX, SOCK_DGRAM, 0);
    assert(server >= 0);

    struct sockaddr_un address;
    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strcpy(address.sun_path, FLIP_GPU_SOCKET_PATH);

    assert(bind(
        server,
        (const struct sockaddr *)&address,
        sizeof(address)) == 0);

    flip_gpu_client_t client;
    assert(flip_gpu_client_open(&client) == 0);

    const unsigned short pixels[4] = {
        0xf800,
        0x07e0,
        0x001f,
        0xffff
    };

    assert(flip_gpu_client_submit(
        &client,
        pixels,
        2,
        2,
        4,
        FLIP_GPU_FORMAT_RGB565) == 0);

    unsigned char packet[
        sizeof(flip_gpu_frame_header_t) +
        sizeof(pixels)];

    ssize_t received = recv(
        server,
        packet,
        sizeof(packet),
        0);

    assert(received == (ssize_t)sizeof(packet));

    const flip_gpu_frame_header_t *header =
        (const flip_gpu_frame_header_t *)packet;

    assert(flip_gpu_frame_header_validate(header) == 0);
    assert(header->sequence == 0);
    assert(header->width == 2);
    assert(header->height == 2);
    assert(header->pitch == 4);
    assert(header->payload_bytes == sizeof(pixels));

    assert(memcmp(
        packet + sizeof(*header),
        pixels,
        sizeof(pixels)) == 0);

    assert(client.submitted == 1);
    assert(client.dropped == 0);

    uint32_t overlay_pixels[4] = {
        0x00000000,
        0x00ffffff,
        0x00112233,
        0x00445566
    };

    assert(flip_gpu_client_submit_overlay(
        &client,
        FLIP_GPU_OVERLAY_OSD,
        10,
        20,
        overlay_pixels,
        2,
        2,
        2 * sizeof(uint32_t)) == 0);

    unsigned char overlay_packet[
        sizeof(flip_gpu_overlay_header_t) +
        sizeof(overlay_pixels)];

    received = recv(
        server,
        overlay_packet,
        sizeof(overlay_packet),
        0);

    assert(received == (ssize_t)sizeof(overlay_packet));

    const flip_gpu_overlay_header_t *overlay_header =
        (const flip_gpu_overlay_header_t *)overlay_packet;

    assert(flip_gpu_overlay_header_validate(
        overlay_header) == 0);

    assert(overlay_header->slot ==
        FLIP_GPU_OVERLAY_OSD);
    assert(overlay_header->x == 10);
    assert(overlay_header->y == 20);
    assert(overlay_header->width == 2);
    assert(overlay_header->height == 2);

    assert(memcmp(
        overlay_packet + sizeof(*overlay_header),
        overlay_pixels,
        sizeof(overlay_pixels)) == 0);

    assert(flip_gpu_client_clear_overlay(
        &client,
        FLIP_GPU_OVERLAY_OSD) == 0);

    unsigned char clear_packet[
        sizeof(flip_gpu_overlay_header_t)];

    received = recv(
        server,
        clear_packet,
        sizeof(clear_packet),
        0);

    assert(received == (ssize_t)sizeof(clear_packet));

    overlay_header =
        (const flip_gpu_overlay_header_t *)clear_packet;

    assert(flip_gpu_overlay_header_validate(
        overlay_header) == 0);

    assert(!(overlay_header->flags &
        FLIP_GPU_OVERLAY_ACTIVE));

    assert(client.overlay_submitted == 2);
    assert(client.overlay_dropped == 0);

    flip_gpu_client_close(&client);
    close(server);
    unlink(FLIP_GPU_SOCKET_PATH);

    flip_gpu_client_t launched;
    assert(flip_gpu_client_launch(
        &launched,
        TEST_GPU_SERVICE_PATH,
        1) == 0);
    assert(launched.service_pid > 0);

    assert(flip_gpu_client_submit(
        &launched,
        pixels,
        2,
        2,
        4,
        FLIP_GPU_FORMAT_RGB565) == 0);

    flip_gpu_client_close(&launched);
    assert(launched.fd < 0);
    assert(launched.service_pid < 0);

    puts("flip_gpu_client tests passed");
    return 0;
}
