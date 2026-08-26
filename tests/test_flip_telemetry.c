#include "flip_telemetry.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void test_cpu_delta(void)
{
    double percent = 0.0;

    assert(flip_telemetry_cpu_percent(
               1000, 700,
               1100, 750,
               &percent) == 0);

    assert(fabs(percent - 50.0) < 0.001);

    assert(flip_telemetry_cpu_percent(
               1000, 700,
               1000, 700,
               &percent) < 0);
}

static void test_overlay_format(void)
{
    flip_telemetry_snapshot_t snapshot;
    memset(&snapshot, 0, sizeof(snapshot));

    snapshot.fps = 59.73;
    snapshot.fps_valid = 1;

    snapshot.cpu_percent = 42.0;
    snapshot.cpu_percent_valid = 1;
    snapshot.cpu_khz = 1800000;
    snapshot.cpu_khz_valid = 1;

    snapshot.gpu_percent = 31.0;
    snapshot.gpu_percent_valid = 1;
    snapshot.gpu_hz = 600000000;
    snapshot.gpu_hz_valid = 1;

    snapshot.temp_millic = 54000;
    snapshot.temp_valid = 1;

    snapshot.battery_percent = 73;
    snapshot.battery_valid = 1;

    char lines[FLIP_TELEMETRY_OVERLAY_LINES][FLIP_TELEMETRY_OVERLAY_LINE_LENGTH];

    flip_telemetry_format_overlay(&snapshot, lines);

    assert(strcmp(lines[0], "FPS 59.7") == 0);
    assert(strcmp(lines[1], "CPU 42% 1800M") == 0);
    assert(strcmp(lines[2], "GPU 31% 600M") == 0);
    assert(strcmp(lines[3], "TMP 54C") == 0);
    assert(strcmp(lines[4], "BAT 73%") == 0);
}

static void test_unavailable_values(void)
{
    flip_telemetry_snapshot_t snapshot;
    memset(&snapshot, 0, sizeof(snapshot));

    char lines[FLIP_TELEMETRY_OVERLAY_LINES][FLIP_TELEMETRY_OVERLAY_LINE_LENGTH];

    flip_telemetry_format_overlay(&snapshot, lines);

    assert(strcmp(lines[0], "FPS --") == 0);
    assert(strcmp(lines[1], "CPU --") == 0);
    assert(strcmp(lines[2], "GPU --") == 0);
    assert(strcmp(lines[3], "TMP --") == 0);
    assert(strcmp(lines[4], "BAT --") == 0);
}

int main(void)
{
    test_cpu_delta();
    test_overlay_format();
    test_unavailable_values();

    printf("flip_telemetry tests passed\n");
    return 0;
}
