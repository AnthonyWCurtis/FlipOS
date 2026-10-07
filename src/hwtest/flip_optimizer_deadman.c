#include <errno.h>
#include <linux/reboot.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#define FLIP_DEADMAN_MIN_SECONDS 5U
#define FLIP_DEADMAN_MAX_SECONDS 180U
#define FLIP_DEADMAN_STACK_TOUCH_BYTES (64U * 1024U)

static unsigned char locked_probe[FLIP_DEADMAN_STACK_TOUCH_BYTES];

static int parse_unsigned(const char *text, unsigned *value)
{
    if (!text || !text[0] || !value)
        return -1;

    errno = 0;
    char *end = NULL;
    unsigned long parsed = strtoul(text, &end, 10);
    if (errno || end == text || *end != '\0' || parsed > 0xffffffffUL)
        return -1;

    *value = (unsigned)parsed;
    return 0;
}

static int add_seconds(struct timespec *deadline, unsigned seconds)
{
    if (!deadline)
        return -1;

    if ((unsigned long long)deadline->tv_sec + seconds > 0x7fffffffULL)
        return -1;

    deadline->tv_sec += (time_t)seconds;
    return 0;
}

static int arm_realtime_runtime(void)
{
    memset(locked_probe, 0xa5, sizeof(locked_probe));

    if (mlockall(MCL_CURRENT | MCL_FUTURE) < 0)
        return -1;

    int priority = sched_get_priority_max(SCHED_FIFO);
    if (priority < 1)
        return -1;

    struct sched_param param = { .sched_priority = priority };
    if (sched_setscheduler(0, SCHED_FIFO, &param) < 0)
        return -1;

    return 0;
}

static void direct_reboot(void)
{
    static const char mode[] = "watchdog";

    syscall(
        SYS_reboot,
        LINUX_REBOOT_MAGIC1,
        LINUX_REBOOT_MAGIC2,
        LINUX_REBOOT_CMD_RESTART2,
        mode);
    _exit(125);
}

int main(int argc, char **argv)
{
    if (argc != 4 || strcmp(argv[1], "arm") != 0) {
        fprintf(stderr, "usage: %s arm SECONDS READY_FD\n", argv[0]);
        return EXIT_FAILURE;
    }

    unsigned seconds = 0;
    unsigned ready_fd_value = 0;
    if (parse_unsigned(argv[2], &seconds) < 0 ||
        seconds < FLIP_DEADMAN_MIN_SECONDS ||
        seconds > FLIP_DEADMAN_MAX_SECONDS ||
        parse_unsigned(argv[3], &ready_fd_value) < 0 ||
        ready_fd_value > 0x7fffffffU) {
        return EXIT_FAILURE;
    }

    int ready_fd = (int)ready_fd_value;

    if (arm_realtime_runtime() < 0)
        return EXIT_FAILURE;

    struct timespec deadline;
    if (clock_gettime(CLOCK_MONOTONIC, &deadline) < 0 ||
        add_seconds(&deadline, seconds) < 0)
        return EXIT_FAILURE;

    const char ready = 'R';
    if (write(ready_fd, &ready, 1) != 1)
        return EXIT_FAILURE;
    close(ready_fd);

    for (;;) {
        int rc = clock_nanosleep(
            CLOCK_MONOTONIC,
            TIMER_ABSTIME,
            &deadline,
            NULL);
        if (rc == 0)
            break;
        if (rc != EINTR)
            return EXIT_FAILURE;
    }

    direct_reboot();
}
