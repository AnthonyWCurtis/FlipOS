#define _POSIX_C_SOURCE 200809L

#include "flip_gpu_client.h"
#include "flip_gpu_protocol.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

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

    flip_gpu_client_close(&client);
    close(server);
    unlink(FLIP_GPU_SOCKET_PATH);

    puts("flip_gpu_client tests passed");
    return 0;
}
