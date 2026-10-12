#define _POSIX_C_SOURCE 200809L

#include "flip_audio_transport.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static uint64_t monotonic_ms(void)
{
    struct timespec now;
    assert(clock_gettime(CLOCK_MONOTONIC, &now) == 0);
    return
        (uint64_t)now.tv_sec * 1000ULL +
        (uint64_t)now.tv_nsec / 1000000ULL;
}

static void test_normal_write(void)
{
    int fds[2];
    assert(pipe(fds) == 0);
    assert(flip_audio_transport_set_nonblocking(fds[1]) == 0);

    const char payload[] = "flip-audio";
    assert(flip_audio_transport_write_all(
        fds[1], payload, sizeof(payload), 100) == 0);

    char got[sizeof(payload)];
    assert(read(fds[0], got, sizeof(got)) == (ssize_t)sizeof(got));
    assert(memcmp(got, payload, sizeof(got)) == 0);

    close(fds[0]);
    close(fds[1]);
}

static void test_stalled_pipe_times_out(void)
{
    int fds[2];
    assert(pipe(fds) == 0);
    assert(flip_audio_transport_set_nonblocking(fds[1]) == 0);

    char fill[4096];
    memset(fill, 0x5a, sizeof(fill));

    for (;;) {
        ssize_t rc = write(fds[1], fill, sizeof(fill));
        if (rc > 0)
            continue;

        assert(rc < 0);
        assert(errno == EAGAIN || errno == EWOULDBLOCK);
        break;
    }

    const char more[] = "blocked";
    uint64_t start = monotonic_ms();
    errno = 0;
    assert(flip_audio_transport_write_all(
        fds[1], more, sizeof(more), 40) < 0);
    uint64_t elapsed = monotonic_ms() - start;

    assert(errno == ETIMEDOUT);
    assert(elapsed >= 20);
    assert(elapsed < 500);

    close(fds[0]);
    close(fds[1]);
}

static void test_clean_child_exit(void)
{
    pid_t pid = fork();
    assert(pid >= 0);

    if (pid == 0)
        _exit(7);

    int status = 0;
    assert(flip_audio_transport_shutdown_child(
        pid, 200, 50, 100, &status) == 0);
    assert(WIFEXITED(status));
    assert(WEXITSTATUS(status) == 7);
}

static void test_stubborn_child_is_bounded(void)
{
    int ready[2];
    assert(pipe(ready) == 0);

    pid_t pid = fork();
    assert(pid >= 0);

    if (pid == 0) {
        close(ready[0]);
        signal(SIGTERM, SIG_IGN);
        assert(write(ready[1], "R", 1) == 1);
        close(ready[1]);

        for (;;)
            pause();
    }

    close(ready[1]);
    char marker = 0;
    assert(read(ready[0], &marker, 1) == 1);
    close(ready[0]);
    assert(marker == 'R');

    uint64_t start = monotonic_ms();
    int status = 0;
    assert(flip_audio_transport_shutdown_child(
        pid, 20, 30, 200, &status) == 0);
    uint64_t elapsed = monotonic_ms() - start;

    assert(WIFSIGNALED(status));
    assert(WTERMSIG(status) == SIGKILL);
    assert(elapsed < 500);
}

int main(void)
{
    test_normal_write();
    test_stalled_pipe_times_out();
    test_clean_child_exit();
    test_stubborn_child_is_bounded();

    puts("flip_audio_transport tests passed");
    return 0;
}
