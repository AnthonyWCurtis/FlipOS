#define _GNU_SOURCE

#include "flip_gpu_client.h"

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static void client_reset(
    flip_gpu_client_t *client)
{
    memset(
        client,
        0,
        sizeof(*client));

    client->fd = -1;
    client->service_pid = -1;
}

static int client_connect_socket(
    flip_gpu_client_t *client)
{
    int fd = socket(
        AF_UNIX,
        SOCK_DGRAM |
        SOCK_NONBLOCK |
        SOCK_CLOEXEC,
        0);

    if (fd < 0)
        return -1;

    int send_buffer =
        512 * 1024;

    (void)setsockopt(
        fd,
        SOL_SOCKET,
        SO_SNDBUF,
        &send_buffer,
        sizeof(send_buffer));

    struct sockaddr_un address;
    memset(
        &address,
        0,
        sizeof(address));

    address.sun_family =
        AF_UNIX;

    if (strlen(FLIP_GPU_SOCKET_PATH) >=
        sizeof(address.sun_path)) {

        close(fd);
        return -1;
    }

    strcpy(
        address.sun_path,
        FLIP_GPU_SOCKET_PATH);

    if (connect(
            fd,
            (const struct sockaddr *)&address,
            sizeof(address)) < 0) {

        close(fd);
        return -1;
    }

    client->fd = fd;
    return 0;
}

int flip_gpu_client_open(
    flip_gpu_client_t *client)
{
    if (!client)
        return -1;

    client_reset(client);

    return client_connect_socket(
        client);
}

int flip_gpu_client_launch(
    flip_gpu_client_t *client,
    const char *service_path,
    int scanout)
{
    if (!client ||
        !service_path ||
        !service_path[0]) {

        return -1;
    }

    client_reset(client);

    pid_t pid = fork();

    if (pid < 0)
        return -1;

    if (pid == 0) {
        if (scanout) {
            execl(
                service_path,
                service_path,
                "--scanout",
                (char *)NULL);
        } else {
            execl(
                service_path,
                service_path,
                (char *)NULL);
        }

        _exit(127);
    }

    client->service_pid = pid;

    const struct timespec delay = {
        .tv_sec = 0,
        .tv_nsec = 10000000L
    };

    for (unsigned attempt = 0;
         attempt < 100;
         ++attempt) {

        if (client_connect_socket(client) == 0)
            return 0;

        int status = 0;
        pid_t wait_result =
            waitpid(
                pid,
                &status,
                WNOHANG);

        if (wait_result == pid) {
            client->service_pid = -1;
            return -1;
        }

        struct timespec remaining =
            delay;

        while (nanosleep(
                   &remaining,
                   &remaining) < 0 &&
               errno == EINTR) {
        }
    }

    (void)kill(
        pid,
        SIGTERM);

    while (waitpid(
               pid,
               NULL,
               0) < 0 &&
           errno == EINTR) {
    }

    client->service_pid = -1;
    return -1;
}

void flip_gpu_client_close(
    flip_gpu_client_t *client)
{
    if (!client)
        return;

    if (client->fd >= 0) {
        close(client->fd);
        client->fd = -1;
    }

    if (client->service_pid > 0) {
        (void)kill(
            client->service_pid,
            SIGTERM);

        while (waitpid(
                   client->service_pid,
                   NULL,
                   0) < 0 &&
               errno == EINTR) {
        }

        client->service_pid = -1;
    }
}

int flip_gpu_client_submit(
    flip_gpu_client_t *client,
    const void *data,
    unsigned width,
    unsigned height,
    size_t pitch,
    flip_gpu_format_t format)
{
    if (!client ||
        client->fd < 0 ||
        !data) {

        return -1;
    }

    flip_gpu_frame_header_t header;

    if (flip_gpu_frame_header_init(
            &header,
            format,
            width,
            height,
            pitch,
            client->sequence++) < 0) {

        return -1;
    }

    struct iovec vectors[2] = {
        {
            .iov_base = &header,
            .iov_len = sizeof(header)
        },
        {
            .iov_base = (void *)data,
            .iov_len = header.payload_bytes
        }
    };

    struct msghdr message;
    memset(
        &message,
        0,
        sizeof(message));

    message.msg_iov = vectors;
    message.msg_iovlen = 2;

    ssize_t sent = sendmsg(
        client->fd,
        &message,
        MSG_DONTWAIT |
        MSG_NOSIGNAL);

    if (sent ==
        (ssize_t)(
            sizeof(header) +
            header.payload_bytes)) {

        client->submitted++;
        return 0;
    }

    if (sent < 0 &&
        (errno == EAGAIN ||
         errno == EWOULDBLOCK ||
         errno == ENOBUFS)) {

        client->dropped++;
        return 1;
    }

    return -1;
}

static int send_overlay_packet(
    flip_gpu_client_t *client,
    const flip_gpu_overlay_header_t *header,
    const void *data)
{
    if (!client ||
        client->fd < 0 ||
        !header) {

        return -1;
    }

    struct iovec vectors[2] = {
        {
            .iov_base = (void *)header,
            .iov_len = sizeof(*header)
        },
        {
            .iov_base = (void *)data,
            .iov_len = header->payload_bytes
        }
    };

    struct msghdr message;
    memset(
        &message,
        0,
        sizeof(message));

    message.msg_iov = vectors;
    message.msg_iovlen =
        header->payload_bytes ? 2 : 1;

    ssize_t sent = sendmsg(
        client->fd,
        &message,
        MSG_DONTWAIT |
        MSG_NOSIGNAL);

    size_t expected =
        sizeof(*header) +
        header->payload_bytes;

    if (sent ==
        (ssize_t)expected) {

        client->overlay_submitted++;
        return 0;
    }

    if (sent < 0 &&
        (errno == EAGAIN ||
         errno == EWOULDBLOCK ||
         errno == ENOBUFS)) {

        client->overlay_dropped++;
        return 1;
    }

    return -1;
}

int flip_gpu_client_submit_overlay(
    flip_gpu_client_t *client,
    flip_gpu_overlay_slot_t slot,
    int x,
    int y,
    const void *data,
    unsigned width,
    unsigned height,
    size_t pitch)
{
    if (!data)
        return -1;

    flip_gpu_overlay_header_t header;

    if (flip_gpu_overlay_header_init(
            &header,
            slot,
            x,
            y,
            width,
            height,
            pitch) < 0) {

        return -1;
    }

    return send_overlay_packet(
        client,
        &header,
        data);
}

int flip_gpu_client_clear_overlay(
    flip_gpu_client_t *client,
    flip_gpu_overlay_slot_t slot)
{
    flip_gpu_overlay_header_t header;

    if (flip_gpu_overlay_header_clear(
            &header,
            slot) < 0) {

        return -1;
    }

    return send_overlay_packet(
        client,
        &header,
        NULL);
}
