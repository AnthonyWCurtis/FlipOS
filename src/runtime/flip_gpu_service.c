#define _POSIX_C_SOURCE 200809L

#include "flip_gpu_context.h"
#include "flip_gpu_kms.h"
#include "flip_gpu_protocol.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t stop_requested = 0;

static void handle_signal(int signal_number)
{
    (void)signal_number;
    stop_requested = 1;
}

static uint64_t monotonic_us(void)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0)
        return 0;
    return (uint64_t)now.tv_sec * 1000000ULL + (uint64_t)now.tv_nsec / 1000ULL;
}

int main(void)
{
    int rc = 1;
    int socket_fd = -1;
    int kms_fd = -1;
    unsigned char *payload = NULL;
    flip_gpu_context_t *gpu = NULL;

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    kms_fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (kms_fd < 0) {
        perror("FLIP_GPU_SERVICE: open card0");
        goto out;
    }

    flip_gpu_kms_info_t kms_info;
    if (flip_gpu_kms_discover(kms_fd, &kms_info) < 0) {
        fprintf(stderr, "FLIP_GPU_SERVICE: KMS discovery failed\n");
        goto out;
    }

    close(kms_fd);
    kms_fd = -1;

    gpu = flip_gpu_context_create(kms_info.width, kms_info.height);
    if (!gpu)
        goto out;

    const flip_gpu_context_info_t *info = flip_gpu_context_info(gpu);
    fprintf(stderr,
        "FLIP_GPU_SERVICE: GPU_READY renderer=%s\n",
        info && info->gl_renderer ? info->gl_renderer : "?");

    socket_fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (socket_fd < 0) {
        perror("FLIP_GPU_SERVICE: socket");
        goto out;
    }

    int receive_buffer = 1024 * 1024;
    (void)setsockopt(
        socket_fd,
        SOL_SOCKET,
        SO_RCVBUF,
        &receive_buffer,
        sizeof(receive_buffer));

    struct sockaddr_un address;
    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strcpy(address.sun_path, FLIP_GPU_SOCKET_PATH);

    unlink(FLIP_GPU_SOCKET_PATH);
    if (bind(
            socket_fd,
            (const struct sockaddr *)&address,
            sizeof(address)) < 0) {
        perror("FLIP_GPU_SERVICE: bind");
        goto out;
    }

    payload = malloc(FLIP_GPU_MAX_PAYLOAD);
    if (!payload)
        goto out;

    fprintf(stderr, "FLIP_GPU_SERVICE: LISTENING socket=%s\n", FLIP_GPU_SOCKET_PATH);

    uint64_t frames = 0;
    uint64_t invalid = 0;
    uint64_t upload_total_us = 0;
    uint64_t upload_max_us = 0;
    uint64_t interval_start_us = monotonic_us();

    while (!stop_requested) {
        flip_gpu_frame_header_t header;
        struct iovec vectors[2] = {
            { .iov_base = &header, .iov_len = sizeof(header) },
            { .iov_base = payload, .iov_len = FLIP_GPU_MAX_PAYLOAD }
        };
        struct msghdr message;
        memset(&message, 0, sizeof(message));
        message.msg_iov = vectors;
        message.msg_iovlen = 2;

        ssize_t received = recvmsg(socket_fd, &message, 0);
        if (received < 0) {
            if (errno == EINTR)
                continue;
            perror("FLIP_GPU_SERVICE: recvmsg");
            goto out;
        }

        if ((size_t)received < sizeof(header) ||
            flip_gpu_frame_header_validate(&header) < 0 ||
            (size_t)received != sizeof(header) + header.payload_bytes ||
            (message.msg_flags & MSG_TRUNC)) {
            invalid++;
            continue;
        }

        uint64_t upload_start_us = monotonic_us();
        if (flip_gpu_context_render(
                gpu,
                payload,
                header.width,
                header.height,
                header.pitch,
                (flip_gpu_format_t)header.format) < 0) {
            fprintf(stderr, "FLIP_GPU_SERVICE: upload failed sequence=%llu\n",
                (unsigned long long)header.sequence);
            goto out;
        }

        uint64_t upload_us = monotonic_us() - upload_start_us;
        upload_total_us += upload_us;
        if (upload_us > upload_max_us)
            upload_max_us = upload_us;
        frames++;

        uint64_t now_us = monotonic_us();
        if (now_us - interval_start_us >= 1000000ULL) {
            fprintf(stderr,
                "FLIP_GPU_SERVICE_PERF: frames=%llu render_avg_us=%llu render_max_us=%llu invalid=%llu\n",
                (unsigned long long)frames,
                (unsigned long long)(frames ? upload_total_us / frames : 0),
                (unsigned long long)upload_max_us,
                (unsigned long long)invalid);
            frames = 0;
            invalid = 0;
            upload_total_us = 0;
            upload_max_us = 0;
            interval_start_us = now_us;
        }
    }

    rc = 0;

out:
    if (socket_fd >= 0)
        close(socket_fd);
    if (kms_fd >= 0)
        close(kms_fd);
    unlink(FLIP_GPU_SOCKET_PATH);
    free(payload);
    flip_gpu_context_destroy(gpu);
    fprintf(stderr, "FLIP_GPU_SERVICE: STOP rc=%d\n", rc);
    return rc;
}
