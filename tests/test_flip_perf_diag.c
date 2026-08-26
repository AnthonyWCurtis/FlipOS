#include "flip_perf_diag.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    flip_perf_diag_t diag;

    flip_perf_diag_reset(&diag);

    flip_perf_diag_record_frame(
        &diag,
        100,
        20,
        5,
        2000,
        700,
        300,
        3500,
        0,
        0);

    flip_perf_diag_record_frame(
        &diag,
        200,
        30,
        10,
        3000,
        900,
        500,
        5000,
        18000,
        1);

    assert(diag.frames == 2);

    assert(diag.input.sum_us == 300);
    assert(diag.input.max_us == 200);

    assert(diag.core_run.sum_us == 5000);
    assert(diag.core_run.max_us == 3000);

    assert(diag.present.sum_us == 1600);
    assert(diag.present.max_us == 900);

    assert(diag.audio.sum_us == 800);
    assert(diag.audio.max_us == 500);

    assert(diag.host.sum_us == 8500);
    assert(diag.host.max_us == 5000);

    assert(diag.late_frames == 1);
    assert(diag.late.sum_us == 18000);
    assert(diag.late.max_us == 18000);
    assert(diag.resyncs == 1);

    assert(
        flip_perf_metric_avg_us(
            &diag.core_run,
            diag.frames) == 2500);

    puts("flip_perf_diag tests passed");
    return 0;
}
