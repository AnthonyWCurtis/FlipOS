#ifndef FLIP_AUDIO_BUFFER_H
#define FLIP_AUDIO_BUFFER_H

#include <stddef.h>
#include <stdint.h>

#define FLIP_AUDIO_BUFFER_FRAMES 256
#define FLIP_AUDIO_BUFFER_CHANNELS 2

typedef int (*flip_audio_buffer_sink_fn)(
    const int16_t *samples,
    size_t frames,
    void *userdata);

typedef struct {
    int16_t samples[
        FLIP_AUDIO_BUFFER_FRAMES *
        FLIP_AUDIO_BUFFER_CHANNELS];
    size_t frames;
} flip_audio_buffer_t;

void flip_audio_buffer_init(
    flip_audio_buffer_t *buffer);

size_t flip_audio_buffer_submit(
    flip_audio_buffer_t *buffer,
    const int16_t *samples,
    size_t frames,
    flip_audio_buffer_sink_fn sink,
    void *userdata,
    int *sink_failed);

int flip_audio_buffer_flush(
    flip_audio_buffer_t *buffer,
    flip_audio_buffer_sink_fn sink,
    void *userdata);

void flip_audio_buffer_reset(
    flip_audio_buffer_t *buffer);

#endif
