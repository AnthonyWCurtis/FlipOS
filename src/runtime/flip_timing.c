#define _POSIX_C_SOURCE 200809L

#include "flip_timing.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>

static void timespec_add_ns(
    struct timespec *value,
    long long ns)
{
    value->tv_sec +=
        (time_t)(ns / 1000000000LL);

    value->tv_nsec +=
        (long)(ns % 1000000000LL);

    if (value->tv_nsec >= 1000000000L) {
        value->tv_nsec -= 1000000000L;
        value->tv_sec++;
    }
}

static int timespec_after(
    const struct timespec *a,
    const struct timespec *b)
{
    if (a->tv_sec != b->tv_sec)
        return a->tv_sec > b->tv_sec;

    return a->tv_nsec > b->tv_nsec;
}

static double timespec_elapsed_seconds(
    const struct timespec *start,
    const struct timespec *end)
{
    return
        (double)(end->tv_sec - start->tv_sec) +
        (double)(end->tv_nsec - start->tv_nsec) /
        1000000000.0;
}

int flip_timing_resync(
    flip_timing_t *timing)
{
    struct timespec now;

    if (!timing)
        return -1;

    if (clock_gettime(
            CLOCK_MONOTONIC,
            &now) < 0)
        return -1;

    timing->next_frame = now;
    timing->fps_started = now;
    timing->fps_frames = 0;
    timing->measured_fps = 0.0;

    return 0;
}

int flip_timing_init(
    flip_timing_t *timing,
    double target_fps)
{
    double period;

    if (!timing ||
        !isfinite(target_fps) ||
        target_fps <= 0.0)
        return -1;

    period =
        1000000000.0 /
        target_fps;

    if (!isfinite(period) ||
        period < 1.0 ||
        period > (double)LLONG_MAX)
        return -1;

    timing->target_fps =
        target_fps;

    timing->frame_period_ns =
        (long long)(period + 0.5);

    return flip_timing_resync(
        timing);
}

int flip_timing_record_frame(
    flip_timing_t *timing,
    double *measured_fps)
{
    struct timespec now;
    double elapsed;

    if (!timing ||
        !measured_fps)
        return -1;

    if (clock_gettime(
            CLOCK_MONOTONIC,
            &now) < 0)
        return -1;

    timing->fps_frames++;

    elapsed =
        timespec_elapsed_seconds(
            &timing->fps_started,
            &now);

    if (elapsed < 1.0) {
        *measured_fps =
            timing->measured_fps;

        return 0;
    }

    timing->measured_fps =
        (double)timing->fps_frames /
        elapsed;

    timing->fps_frames = 0;
    timing->fps_started = now;

    *measured_fps =
        timing->measured_fps;

    return 1;
}

int flip_timing_wait_frame(
    flip_timing_t *timing)
{
    struct timespec now;
    struct timespec late_limit;
    int sleep_result;

    if (!timing ||
        timing->frame_period_ns <= 0)
        return -1;

    timespec_add_ns(
        &timing->next_frame,
        timing->frame_period_ns);

    do {
        sleep_result =
            clock_nanosleep(
                CLOCK_MONOTONIC,
                TIMER_ABSTIME,
                &timing->next_frame,
                NULL);
    } while (sleep_result == EINTR);

    if (sleep_result != 0)
        return -1;

    if (clock_gettime(
            CLOCK_MONOTONIC,
            &now) < 0)
        return -1;

    late_limit =
        timing->next_frame;

    timespec_add_ns(
        &late_limit,
        timing->frame_period_ns);

    if (timespec_after(
            &now,
            &late_limit)) {

        timing->next_frame = now;
        return 1;
    }

    return 0;
}
