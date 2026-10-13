#define _POSIX_C_SOURCE 200809L

#include "flip_gpu_protocol.h"

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static volatile sig_atomic_t stop_requested = 0;

static void stop_handler(int signal_number)
{
    (void)signal_number;
    stop_requested = 1;
}


static int signal_parent_ready(void)
{
    const char *text = getenv(FLIP_GPU_READY_FD_ENV);
    if (!text || !text[0])
        return 0;

    errno = 0;
    char *end = NULL;
    long parsed = strtol(text, &end, 10);
    if (errno || end == text || *end != '\0' ||
        parsed < 0 || parsed > 0x7fffffffL)
        return -1;

    int fd = (int)parsed;
    unsigned char ready = FLIP_GPU_READY_BYTE;
    ssize_t written = write(fd, &ready, sizeof(ready));
    close(fd);
    unsetenv(FLIP_GPU_READY_FD_ENV);
    return written == 1 ? 0 : -1;
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    signal(SIGTERM, stop_handler);
    signal(SIGINT, stop_handler);

    int fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd < 0)
        return 1;

    struct sockaddr_un address;
    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strcpy(address.sun_path, FLIP_GPU_SOCKET_PATH);

    unlink(FLIP_GPU_SOCKET_PATH);

    if (bind(
            fd,
            (const struct sockaddr *)&address,
            sizeof(address)) < 0) {

        close(fd);
        return 1;
    }

    if (signal_parent_ready() < 0) {
        close(fd);
        unlink(FLIP_GPU_SOCKET_PATH);
        return 1;
    }

    while (!stop_requested)
        pause();

    close(fd);
    unlink(FLIP_GPU_SOCKET_PATH);
    return 0;
}
