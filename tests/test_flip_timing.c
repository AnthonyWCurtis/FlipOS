#define _POSIX_C_SOURCE 200809L

#include "flip_timing.h"

#include <assert.h>
#include <stdio.h>
#include <time.h>

static double abs_double(
    double value)
{
    return value < 0.0
        ? -value
        : value;
}

static void subtract_seconds(
    struct timespec *value,
    time_t seconds)
{
    value->tv_sec -= seconds;
}

int main(void)
{
    flip_timing_t timing;
    double measured = 0.0;

    assert(
        flip_timing_init(
            &timing,
            0.0) < 0);

    assert(
        flip_timing_init(
            &timing,
            -1.0) < 0);

    assert(
        flip_timing_init(
            &timing,
            59.7275) == 0);

    assert(
        timing.frame_period_ns ==
        16742706LL);

    /*
     * Force an elapsed FPS window without making the
     * host test sleep for a real second.
     */
    assert(
        clock_gettime(
            CLOCK_MONOTONIC,
            &timing.fps_started) == 0);

    subtract_seconds(
        &timing.fps_started,
        1);

    timing.fps_frames = 59;

    assert(
        flip_timing_record_frame(
            &timing,
            &measured) == 1);

    assert(
        abs_double(
            measured - 60.0) < 0.5);

    assert(
        timing.fps_frames == 0);

    /*
     * A deadline many seconds in the past must return
     * immediately and restart the absolute schedule.
     */
    assert(
        clock_gettime(
            CLOCK_MONOTONIC,
            &timing.next_frame) == 0);

    subtract_seconds(
        &timing.next_frame,
        5);

    assert(
        flip_timing_wait_frame(
            &timing) == 1);

    timing.fps_frames = 42;
    timing.measured_fps = 42.0;

    assert(
        flip_timing_resync(
            &timing) == 0);

    assert(
        timing.fps_frames == 0);

    assert(
        timing.measured_fps == 0.0);

    puts(
        "flip_timing tests passed");

    return 0;
}
