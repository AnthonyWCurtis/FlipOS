#define _GNU_SOURCE

#include "flip_gpu_client.h"

#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

int flip_gpu_client_open(flip_gpu_client_t *client)
{
    if (!client)
        return -1;

    memset(client, 0, sizeof(*client));
    client->fd = -1;

    int fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0)
        return -1;

    int send_buffer = 512 * 1024;
    (void)setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &send_buffer, sizeof(send_buffer));

    struct sockaddr_un address;
    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;

    if (strlen(FLIP_GPU_SOCKET_PATH) >= sizeof(address.sun_path)) {
        close(fd);
        return -1;
    }

    strcpy(address.sun_path, FLIP_GPU_SOCKET_PATH);

    if (connect(fd, (const struct sockaddr *)&address, sizeof(address)) < 0) {
        close(fd);
        return -1;
    }

    client->fd = fd;
    return 0;
}

void flip_gpu_client_close(flip_gpu_client_t *client)
{
    if (!client)
        return;

    if (client->fd >= 0)
        close(client->fd);

    client->fd = -1;
}

int flip_gpu_client_submit(
    flip_gpu_client_t *client,
    const void *data,
    unsigned width,
    unsigned height,
    size_t pitch,
    flip_gpu_format_t format)
{
    if (!client || client->fd < 0 || !data)
        return -1;

    flip_gpu_frame_header_t header;
    if (flip_gpu_frame_header_init(
            &header,
            format,
            width,
            height,
            pitch,
            client->sequence++ ) < 0) {
        return -1;
    }

    struct iovec vectors[2] = {
        { .iov_base = &header, .iov_len = sizeof(header) },
        { .iov_base = (void *)data, .iov_len = header.payload_bytes }
    };

    struct msghdr message;
    memset(&message, 0, sizeof(message));
    message.msg_iov = vectors;
    message.msg_iovlen = 2;

    ssize_t sent = sendmsg(client->fd, &message, MSG_DONTWAIT | MSG_NOSIGNAL);
    if (sent == (ssize_t)(sizeof(header) + header.payload_bytes)) {
        client->submitted++;
        return 0;
    }

    if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == ENOBUFS)) {
        client->dropped++;
        return 1;
    }

    return -1;
}
