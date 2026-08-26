#ifndef FLIP_TIMING_H
#define FLIP_TIMING_H

#include <time.h>

typedef struct {
    double target_fps;
    long long frame_period_ns;
    struct timespec next_frame;
    struct timespec fps_started;
    unsigned long long fps_frames;
    double measured_fps;

    /* Diagnostic only: lateness relative to the last absolute deadline. */
    long long last_late_ns;
} flip_timing_t;

int flip_timing_init(flip_timing_t *timing, double target_fps);
int flip_timing_record_frame(flip_timing_t *timing, double *measured_fps);
int flip_timing_wait_frame(flip_timing_t *timing);
int flip_timing_resync(flip_timing_t *timing);

#endif
