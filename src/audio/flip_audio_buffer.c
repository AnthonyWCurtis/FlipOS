#include "flip_audio_buffer.h"

#include <string.h>

void flip_audio_buffer_init(
    flip_audio_buffer_t *buffer)
{
    if (!buffer)
        return;

    memset(
        buffer,
        0,
        sizeof(*buffer));
}

int flip_audio_buffer_flush(
    flip_audio_buffer_t *buffer,
    flip_audio_buffer_sink_fn sink,
    void *userdata)
{
    if (!buffer ||
        !sink)
        return -1;

    if (buffer->frames == 0)
        return 0;

    if (sink(
            buffer->samples,
            buffer->frames,
            userdata) < 0) {

        return -1;
    }

    buffer->frames = 0;
    return 0;
}

size_t flip_audio_buffer_submit(
    flip_audio_buffer_t *buffer,
    const int16_t *samples,
    size_t frames,
    flip_audio_buffer_sink_fn sink,
    void *userdata,
    int *sink_failed)
{
    if (sink_failed)
        *sink_failed = 0;

    if (!buffer ||
        !samples ||
        frames == 0 ||
        !sink) {

        return 0;
    }

    size_t accepted = 0;

    while (accepted < frames) {

        size_t available =
            FLIP_AUDIO_BUFFER_FRAMES -
            buffer->frames;

        size_t count =
            frames - accepted;

        if (count > available)
            count = available;

        memcpy(
            &buffer->samples[
                buffer->frames *
                FLIP_AUDIO_BUFFER_CHANNELS],
            &samples[
                accepted *
                FLIP_AUDIO_BUFFER_CHANNELS],
            count *
                FLIP_AUDIO_BUFFER_CHANNELS *
                sizeof(int16_t));

        buffer->frames += count;
        accepted += count;

        if (buffer->frames ==
                FLIP_AUDIO_BUFFER_FRAMES &&
            flip_audio_buffer_flush(
                buffer,
                sink,
                userdata) < 0) {

            if (sink_failed)
                *sink_failed = 1;

            break;
        }
    }

    return accepted;
}

void flip_audio_buffer_reset(
    flip_audio_buffer_t *buffer)
{
    if (!buffer)
        return;

    buffer->frames = 0;
}
