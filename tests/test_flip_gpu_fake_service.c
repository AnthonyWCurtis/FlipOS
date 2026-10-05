#define _POSIX_C_SOURCE 200809L

#include "flip_gpu_protocol.h"

#include <signal.h>
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

    while (!stop_requested)
        pause();

    close(fd);
    unlink(FLIP_GPU_SOCKET_PATH);
    return 0;
}
