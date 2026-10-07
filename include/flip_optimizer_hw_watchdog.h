#ifndef FLIP_OPTIMIZER_HW_WATCHDOG_H
#define FLIP_OPTIMIZER_HW_WATCHDOG_H

typedef struct {
    int fd;
    unsigned timeout_seconds;
    int touched;
    int armed;
} flip_optimizer_hw_watchdog_t;

/*
 * Opening the RK3566 DesignWare watchdog starts hardware that cannot be
 * physically stopped on the current Miyoo DT/kernel. Once touched is set,
 * the caller must finish by rebooting the device; normal return is unsafe.
 */
int flip_optimizer_hw_watchdog_arm(
    flip_optimizer_hw_watchdog_t *watchdog,
    unsigned requested_timeout_seconds);

int flip_optimizer_hw_watchdog_ping(
    flip_optimizer_hw_watchdog_t *watchdog);

#endif
