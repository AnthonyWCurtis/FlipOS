#ifndef FLIP_AUDIO_QUEUE_H
#define FLIP_AUDIO_QUEUE_H

#include <stddef.h>
#include <stdint.h>

#define FLIP_AUDIO_QUEUE_FRAMES 4096
#define FLIP_AUDIO_QUEUE_CHANNELS 2

typedef struct {
    int16_t samples[
        FLIP_AUDIO_QUEUE_FRAMES *
        FLIP_AUDIO_QUEUE_CHANNELS];

    size_t read_frame;
    size_t write_frame;
    size_t frames;
    size_t peak_frames;
    uint64_t dropped_frames;
} flip_audio_queue_t;

void flip_audio_queue_init(
    flip_audio_queue_t *queue);

size_t flip_audio_queue_write(
    flip_audio_queue_t *queue,
    const int16_t *samples,
    size_t frames);

size_t flip_audio_queue_read(
    flip_audio_queue_t *queue,
    int16_t *samples,
    size_t max_frames);

size_t flip_audio_queue_frames(
    const flip_audio_queue_t *queue);

void flip_audio_queue_reset(
    flip_audio_queue_t *queue);

#endif
