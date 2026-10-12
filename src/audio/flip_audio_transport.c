#define _POSIX_C_SOURCE 200809L

#include "flip_audio_transport.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define TRANSPORT_WAIT_SLICE_MS 10

static uint64_t monotonic_ms(void)
{
    struct timespec now;

    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0)
        return 0;

    return
        (uint64_t)now.tv_sec * 1000ULL +
        (uint64_t)now.tv_nsec / 1000000ULL;
}

static void sleep_ms(unsigned milliseconds)
{
    struct timespec delay = {
        .tv_sec = (time_t)(milliseconds / 1000U),
        .tv_nsec = (long)(milliseconds % 1000U) * 1000000L
    };

    while (nanosleep(&delay, &delay) < 0 &&
           errno == EINTR) {
    }
}

int flip_audio_transport_set_nonblocking(int fd)
{
    if (fd < 0) {
        errno = EINVAL;
        return -1;
    }

    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        return -1;

    if ((flags & O_NONBLOCK) != 0)
        return 0;

    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int flip_audio_transport_write_all(
    int fd,
    const void *data,
    size_t bytes,
    unsigned timeout_ms)
{
    if (fd < 0 || (!data && bytes > 0)) {
        errno = EINVAL;
        return -1;
    }

    if (bytes == 0)
        return 0;

    const unsigned char *cursor = data;
    size_t remaining = bytes;
    uint64_t start_ms = monotonic_ms();
    uint64_t deadline_ms = start_ms + (uint64_t)timeout_ms;

    while (remaining > 0) {
        if (timeout_ms > 0) {
            uint64_t now_ms = monotonic_ms();
            if (now_ms >= deadline_ms) {
                errno = ETIMEDOUT;
                return -1;
            }
        }

        ssize_t written = write(fd, cursor, remaining);
        if (written > 0) {
            cursor += written;
            remaining -= (size_t)written;
            continue;
        }

        if (written < 0 && errno == EINTR)
            continue;

        if (written < 0 &&
            (errno == EAGAIN || errno == EWOULDBLOCK)) {

            int wait_ms = (int)timeout_ms;
            if (timeout_ms > 0) {
                uint64_t now_ms = monotonic_ms();
                if (now_ms >= deadline_ms) {
                    errno = ETIMEDOUT;
                    return -1;
                }

                uint64_t remaining_ms = deadline_ms - now_ms;
                wait_ms =
                    remaining_ms > (uint64_t)INT32_MAX
                        ? INT32_MAX
                        : (int)remaining_ms;
            }

            struct pollfd pfd = {
                .fd = fd,
                .events = POLLOUT,
                .revents = 0
            };

            int rc;
            for (;;) {
                pfd.revents = 0;
                rc = poll(&pfd, 1, wait_ms);

                if (rc >= 0 || errno != EINTR)
                    break;

                /*
                 * A signal storm must not turn a bounded transport wait into
                 * an unbounded one. Recompute the remaining monotonic budget
                 * after every interrupted poll.
                 */
                if (timeout_ms > 0) {
                    uint64_t now_ms = monotonic_ms();
                    if (now_ms >= deadline_ms) {
                        errno = ETIMEDOUT;
                        return -1;
                    }

                    uint64_t remaining_ms = deadline_ms - now_ms;
                    wait_ms =
                        remaining_ms > (uint64_t)INT32_MAX
                            ? INT32_MAX
                            : (int)remaining_ms;
                }
            }

            if (rc == 0) {
                errno = ETIMEDOUT;
                return -1;
            }

            if (rc < 0)
                return -1;

            if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
                errno = EPIPE;
                return -1;
            }

            continue;
        }

        if (written == 0)
            errno = EIO;

        return -1;
    }

    return 0;
}

static int wait_for_child(
    pid_t pid,
    unsigned timeout_ms,
    int *status)
{
    uint64_t deadline_ms =
        monotonic_ms() + (uint64_t)timeout_ms;

    for (;;) {
        int local_status = 0;
        pid_t rc = waitpid(pid, &local_status, WNOHANG);

        if (rc == pid) {
            if (status)
                *status = local_status;
            return 1;
        }

        if (rc < 0) {
            if (errno == EINTR)
                continue;

            if (errno == ECHILD) {
                if (status)
                    *status = 0;
                return 1;
            }

            return -1;
        }

        uint64_t now_ms = monotonic_ms();
        if (now_ms >= deadline_ms)
            return 0;

        uint64_t remaining_ms = deadline_ms - now_ms;
        unsigned delay_ms =
            remaining_ms < TRANSPORT_WAIT_SLICE_MS
                ? (unsigned)remaining_ms
                : TRANSPORT_WAIT_SLICE_MS;

        if (delay_ms == 0)
            delay_ms = 1;

        sleep_ms(delay_ms);
    }
}

int flip_audio_transport_shutdown_child(
    pid_t pid,
    unsigned grace_ms,
    unsigned term_ms,
    unsigned kill_ms,
    int *status)
{
    if (pid <= 0) {
        errno = EINVAL;
        return -1;
    }

    int rc = wait_for_child(pid, grace_ms, status);
    if (rc != 0)
        return rc > 0 ? 0 : -1;

    if (kill(pid, SIGTERM) < 0 &&
        errno != ESRCH) {
        return -1;
    }

    rc = wait_for_child(pid, term_ms, status);
    if (rc != 0)
        return rc > 0 ? 0 : -1;

    if (kill(pid, SIGKILL) < 0 &&
        errno != ESRCH) {
        return -1;
    }

    rc = wait_for_child(pid, kill_ms, status);
    if (rc > 0)
        return 0;

    if (rc == 0)
        errno = ETIMEDOUT;

    return -1;
}
