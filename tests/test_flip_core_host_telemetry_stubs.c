#include "flip_telemetry.h"
#include "flip_settings.h"
#include "flip_osd.h"
#include "flip_video.h"

#include <string.h>

int flip_telemetry_init(
    flip_telemetry_t *telemetry)
{
    if (!telemetry)
        return -1;

    memset(
        telemetry,
        0,
        sizeof(*telemetry));

    return 0;
}

int flip_telemetry_sample(
    flip_telemetry_t *telemetry,
    double fps,
    int fps_valid)
{
    if (!telemetry)
        return -1;

    telemetry->snapshot.fps = fps;
    telemetry->snapshot.fps_valid =
        fps_valid ? 1 : 0;

    return 0;
}

const flip_telemetry_snapshot_t *
flip_telemetry_snapshot(
    const flip_telemetry_t *telemetry)
{
    return telemetry
        ? &telemetry->snapshot
        : NULL;
}

void flip_video_set_perf_overlay(
    int enabled,
    const flip_telemetry_snapshot_t *snapshot)
{
    (void)enabled;
    (void)snapshot;
}

int flip_settings_init(void)
{
    return 0;
}

void flip_osd_init(void)
{
}
