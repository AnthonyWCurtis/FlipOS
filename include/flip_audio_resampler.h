#ifndef FLIP_AUDIO_RESAMPLER_H
#define FLIP_AUDIO_RESAMPLER_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    unsigned source_rate;
    unsigned output_rate;
    unsigned channels;
    uint64_t phase_num;
    int16_t previous[2];
    int have_previous;
} flip_audio_resampler_t;

int flip_audio_resampler_init(
    flip_audio_resampler_t *resampler,
    unsigned source_rate,
    unsigned output_rate,
    unsigned channels);

void flip_audio_resampler_reset(
    flip_audio_resampler_t *resampler);

size_t flip_audio_resampler_process(
    flip_audio_resampler_t *resampler,
    const int16_t *input,
    size_t input_frames,
    size_t *input_consumed,
    int16_t *output,
    size_t output_capacity);

#endif
