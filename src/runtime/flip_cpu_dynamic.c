#include "flip_cpu_dynamic.h"

#include <string.h>

static void sort_frequencies(
    unsigned *values,
    size_t count)
{
    for (size_t i = 1; i < count; ++i) {
        unsigned value = values[i];
        size_t j = i;

        while (j > 0 && values[j - 1] > value) {
            values[j] = values[j - 1];
            --j;
        }

        values[j] = value;
    }
}

static size_t ceiling_index(
    const unsigned *values,
    size_t count,
    unsigned khz)
{
    size_t index = 0;

    for (size_t i = 0; i < count; ++i) {
        if (values[i] > khz)
            break;
        index = i;
    }

    return index;
}

int flip_cpu_dynamic_init(
    flip_cpu_dynamic_t *controller,
    unsigned allowed_max_khz)
{
    if (!controller || !allowed_max_khz)
        return -1;

    memset(controller, 0, sizeof(*controller));

    flip_cpu_policy_info_t info;
    flip_cpu_policy_snapshot_t policy;

    if (flip_cpu_policy_query_info(&info) < 0 ||
        flip_cpu_policy_snapshot(&policy) < 0 ||
        info.frequency_count == 0 ||
        info.frequency_count > FLIP_CPU_POLICY_MAX_FREQS)
        return -1;

    controller->frequency_count = info.frequency_count;
    memcpy(
        controller->frequencies_khz,
        info.frequencies_khz,
        info.frequency_count * sizeof(info.frequencies_khz[0]));

    sort_frequencies(
        controller->frequencies_khz,
        controller->frequency_count);

    controller->current_index =
        ceiling_index(
            controller->frequencies_khz,
            controller->frequency_count,
            policy.max_khz);

    controller->allowed_index =
        ceiling_index(
            controller->frequencies_khz,
            controller->frequency_count,
            allowed_max_khz);

    controller->base_index = controller->current_index;

    controller->current_max_khz =
        controller->frequencies_khz[controller->current_index];

    controller->allowed_max_khz =
        controller->frequencies_khz[controller->allowed_index];

    controller->warmup_windows =
        FLIP_CPU_DYNAMIC_WARMUP_WINDOWS;

    if (controller->allowed_index <= controller->current_index)
        return 0;

    controller->enabled = 1;
    return 1;
}

int flip_cpu_dynamic_sample(
    flip_cpu_dynamic_t *controller,
    double measured_fps,
    double target_fps,
    uint64_t host_avg_us,
    uint64_t late_max_us,
    uint64_t timing_resyncs,
    uint64_t audio_dropped_frames)
{
    if (!controller)
        return -1;

    if (!controller->enabled)
        return 0;

    if (controller->warmup_windows > 0) {
        controller->warmup_windows--;
        return 0;
    }

    if (controller->cooldown_windows > 0)
        controller->cooldown_windows--;

    int lower_probe_active = controller->lower_probe_windows > 0;
    if (controller->lower_probe_windows > 0)
        controller->lower_probe_windows--;

    if (target_fps < 1.0 || measured_fps < 1.0)
        return 0;

    double frame_budget_us =
        1000000.0 / target_fps;

    int fps_bad =
        measured_fps < target_fps - 0.30;

    int severe_fps_bad =
        measured_fps < target_fps * 0.97;

    /*
     * Host time is supporting evidence only. Some healthy full-speed
     * workloads naturally consume almost the entire frame budget; raising
     * clocks solely because host_avg_us is high causes false escalation.
     *
     * Only let host pressure contribute once measured FPS is already
     * measurably below target. Full-speed FPS therefore wins.
     */
    int marginal_fps_bad =
        measured_fps < target_fps * 0.9975;

    int host_pressure =
        marginal_fps_bad &&
        host_avg_us > 0 &&
        (double)host_avg_us >= frame_budget_us * 0.95;

    /*
     * A one-second FPS average can hide a short run of visibly late frames.
     * Treat a worst-frame miss of 30% of the frame budget as pressure, but
     * require the normal two-window hysteresis before raising. A timing
     * resync means we missed by more than a full frame and is severe enough
     * to raise immediately.
     */
    int late_burst_bad =
        late_max_us > 0 &&
        (double)late_max_us >=
            frame_budget_us * 0.30;

    int timing_resync_bad =
        timing_resyncs > 0;

    int audio_bad =
        audio_dropped_frames > 0;

    int bad =
        fps_bad ||
        host_pressure ||
        late_burst_bad ||
        timing_resync_bad ||
        audio_bad;

    if (bad) {
        controller->bad_windows++;
        controller->good_windows = 0;
    } else {
        controller->bad_windows = 0;
        controller->good_windows++;
    }

    int should_raise =
        controller->current_index < controller->allowed_index &&
        (severe_fps_bad ||
         audio_bad ||
         timing_resync_bad ||
         (controller->cooldown_windows == 0 &&
          controller->bad_windows >= FLIP_CPU_DYNAMIC_BAD_WINDOWS_TO_RAISE));

    if (should_raise) {
        size_t next_index = controller->current_index + 1;
        if (next_index > controller->allowed_index)
            next_index = controller->allowed_index;

        unsigned next_khz = controller->frequencies_khz[next_index];
        if (flip_cpu_policy_set_max(next_khz) < 0)
            return -1;

        controller->current_index = next_index;
        controller->current_max_khz = next_khz;
        if (lower_probe_active && next_index > controller->base_index)
            controller->base_index = next_index;
        controller->lower_probe_windows = 0;
        controller->raise_count++;
        controller->bad_windows = 0;
        controller->good_windows = 0;
        controller->cooldown_windows = 3;
        return 1;
    }

    if (!bad &&
        controller->current_index > controller->base_index &&
        controller->cooldown_windows == 0 &&
        controller->good_windows >= FLIP_CPU_DYNAMIC_GOOD_WINDOWS_TO_LOWER) {
        size_t next_index = controller->current_index - 1;
        if (next_index < controller->base_index)
            next_index = controller->base_index;

        unsigned next_khz = controller->frequencies_khz[next_index];
        if (flip_cpu_policy_set_max(next_khz) < 0)
            return -1;

        controller->current_index = next_index;
        controller->current_max_khz = next_khz;
        controller->lower_count++;
        controller->good_windows = 0;
        controller->cooldown_windows = 3;
        controller->lower_probe_windows = FLIP_CPU_DYNAMIC_LOWER_PROBE_WINDOWS;
        return 2;
    }

    return 0;
}
