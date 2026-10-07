#include "flip_optimizer_hw_watchdog.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/watchdog.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#ifndef FLIP_OPTIMIZER_HW_WATCHDOG_PATH
#define FLIP_OPTIMIZER_HW_WATCHDOG_PATH "/dev/watchdog0"
#endif

#define FLIP_OPTIMIZER_HW_WATCHDOG_MIN_SECONDS 5U
#define FLIP_OPTIMIZER_HW_WATCHDOG_MAX_SECONDS 30U

int flip_optimizer_hw_watchdog_arm(
    flip_optimizer_hw_watchdog_t *watchdog,
    unsigned requested_timeout_seconds)
{
    if (!watchdog ||
        requested_timeout_seconds <
            FLIP_OPTIMIZER_HW_WATCHDOG_MIN_SECONDS ||
        requested_timeout_seconds >
            FLIP_OPTIMIZER_HW_WATCHDOG_MAX_SECONDS)
        return -1;

    memset(watchdog, 0, sizeof(*watchdog));
    watchdog->fd = -1;

    int fd = open(
        FLIP_OPTIMIZER_HW_WATCHDOG_PATH,
        O_WRONLY | O_CLOEXEC);
    if (fd < 0)
        return -1;

    /*
     * From this point onward the hardware has been touched. On this board
     * there is no DT reset-control for the DesignWare WDT, so the physical
     * timer cannot be guaranteed to stop again before a full reboot.
     */
    watchdog->fd = fd;
    watchdog->touched = 1;

    struct watchdog_info info;
    memset(&info, 0, sizeof(info));

    if (ioctl(fd, WDIOC_GETSUPPORT, &info) < 0)
        return -1;

    const unsigned required =
        WDIOF_SETTIMEOUT |
        WDIOF_KEEPALIVEPING;

    if ((info.options & required) != required)
        return -1;

    int timeout = (int)requested_timeout_seconds;
    if (ioctl(fd, WDIOC_SETTIMEOUT, &timeout) < 0)
        return -1;

    if (ioctl(fd, WDIOC_GETTIMEOUT, &timeout) < 0 ||
        timeout < (int)FLIP_OPTIMIZER_HW_WATCHDOG_MIN_SECONDS ||
        timeout > (int)FLIP_OPTIMIZER_HW_WATCHDOG_MAX_SECONDS)
        return -1;

    if (ioctl(fd, WDIOC_KEEPALIVE, 0) < 0)
        return -1;

    watchdog->timeout_seconds = (unsigned)timeout;
    watchdog->armed = 1;
    return 0;
}

int flip_optimizer_hw_watchdog_ping(
    flip_optimizer_hw_watchdog_t *watchdog)
{
    if (!watchdog ||
        !watchdog->touched ||
        !watchdog->armed ||
        watchdog->fd < 0)
        return -1;

    return ioctl(
        watchdog->fd,
        WDIOC_KEEPALIVE,
        0) == 0
        ? 0
        : -1;
}
