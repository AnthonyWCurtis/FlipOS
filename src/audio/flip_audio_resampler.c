#include "flip_audio_resampler.h"

#include <limits.h>
#include <string.h>

static int16_t interpolate_sample(
    int16_t a,
    int16_t b,
    uint64_t numerator,
    uint64_t denominator)
{
    int64_t delta =
        (int64_t)b - (int64_t)a;

    int64_t value =
        (int64_t)a +
        (delta * (int64_t)numerator) /
            (int64_t)denominator;

    if (value < INT16_MIN)
        value = INT16_MIN;

    if (value > INT16_MAX)
        value = INT16_MAX;

    return (int16_t)value;
}

int flip_audio_resampler_init(
    flip_audio_resampler_t *resampler,
    unsigned source_rate,
    unsigned output_rate,
    unsigned channels)
{
    if (!resampler ||
        source_rate == 0 ||
        output_rate == 0 ||
        channels != 2) {

        return -1;
    }

    memset(
        resampler,
        0,
        sizeof(*resampler));

    resampler->source_rate = source_rate;
    resampler->output_rate = output_rate;
    resampler->channels = channels;

    return 0;
}

void flip_audio_resampler_reset(
    flip_audio_resampler_t *resampler)
{
    if (!resampler)
        return;

    resampler->phase_num = 0;
    resampler->previous[0] = 0;
    resampler->previous[1] = 0;
    resampler->have_previous = 0;
}

size_t flip_audio_resampler_process(
    flip_audio_resampler_t *resampler,
    const int16_t *input,
    size_t input_frames,
    size_t *input_consumed,
    int16_t *output,
    size_t output_capacity)
{
    size_t consumed = 0;
    size_t produced = 0;

    if (input_consumed)
        *input_consumed = 0;

    if (!resampler ||
        !input_consumed ||
        !input ||
        !output ||
        input_frames == 0 ||
        output_capacity == 0 ||
        resampler->source_rate == 0 ||
        resampler->output_rate == 0 ||
        resampler->channels != 2) {

        return 0;
    }

    if (!resampler->have_previous) {
        output[0] = input[0];
        output[1] = input[1];

        resampler->previous[0] = input[0];
        resampler->previous[1] = input[1];
        resampler->phase_num = resampler->source_rate;
        resampler->have_previous = 1;

        consumed = 1;
        produced = 1;

        if (consumed == input_frames ||
            produced == output_capacity) {

            *input_consumed = consumed;
            return produced;
        }
    }

    while (consumed < input_frames) {
        const int16_t *current =
            input +
            consumed * resampler->channels;

        while (resampler->phase_num <=
               resampler->output_rate) {

            if (produced == output_capacity) {
                *input_consumed = consumed;
                return produced;
            }

            output[produced * 2] =
                interpolate_sample(
                    resampler->previous[0],
                    current[0],
                    resampler->phase_num,
                    resampler->output_rate);

            output[produced * 2 + 1] =
                interpolate_sample(
                    resampler->previous[1],
                    current[1],
                    resampler->phase_num,
                    resampler->output_rate);

            produced++;
            resampler->phase_num +=
                resampler->source_rate;
        }

        resampler->phase_num -=
            resampler->output_rate;

        resampler->previous[0] = current[0];
        resampler->previous[1] = current[1];
        consumed++;
    }

    *input_consumed = consumed;
    return produced;
}
