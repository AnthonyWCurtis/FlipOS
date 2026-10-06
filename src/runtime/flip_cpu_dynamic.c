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
    uint64_t audio_dropped_frames)
{
    if (!controller)
        return -1;

    if (!controller->enabled)
        return 0;

    if (controller->current_index >= controller->allowed_index) {
        controller->enabled = 0;
        return 0;
    }

    if (controller->warmup_windows > 0) {
        controller->warmup_windows--;
        return 0;
    }

    if (controller->cooldown_windows > 0)
        controller->cooldown_windows--;

    if (target_fps < 1.0 || measured_fps < 1.0)
        return 0;

    double frame_budget_us =
        1000000.0 / target_fps;

    int fps_bad =
        measured_fps < target_fps - 0.30;

    int severe_fps_bad =
        measured_fps < target_fps * 0.97;

    int host_pressure =
        host_avg_us > 0 &&
        (double)host_avg_us >= frame_budget_us * 0.95;

    int audio_bad =
        audio_dropped_frames > 0;

    int bad =
        fps_bad ||
        host_pressure ||
        audio_bad;

    if (bad)
        controller->bad_windows++;
    else
        controller->bad_windows = 0;

    int should_raise =
        severe_fps_bad ||
        audio_bad ||
        (controller->cooldown_windows == 0 &&
         controller->bad_windows >= FLIP_CPU_DYNAMIC_BAD_WINDOWS_TO_RAISE);

    if (!should_raise)
        return 0;

    size_t next_index =
        controller->current_index + 1;

    if (next_index > controller->allowed_index)
        next_index = controller->allowed_index;

    unsigned next_khz =
        controller->frequencies_khz[next_index];

    if (flip_cpu_policy_set_max(next_khz) < 0)
        return -1;

    controller->current_index = next_index;
    controller->current_max_khz = next_khz;
    controller->raise_count++;
    controller->bad_windows = 0;
    controller->cooldown_windows = 3;

    if (controller->current_index >= controller->allowed_index)
        controller->enabled = 0;

    return 1;
}
