#include "flip_audio_queue.h"

#include <string.h>

void flip_audio_queue_init(
    flip_audio_queue_t *queue)
{
    if (!queue)
        return;

    memset(
        queue,
        0,
        sizeof(*queue));
}

size_t flip_audio_queue_write(
    flip_audio_queue_t *queue,
    const int16_t *samples,
    size_t frames)
{
    if (!queue ||
        !samples ||
        frames == 0) {

        return 0;
    }

    size_t available =
        FLIP_AUDIO_QUEUE_FRAMES -
        queue->frames;

    size_t accepted =
        frames < available
        ? frames
        : available;

    for (size_t frame = 0;
         frame < accepted;
         ++frame) {

        size_t dst_frame =
            (queue->write_frame + frame) %
            FLIP_AUDIO_QUEUE_FRAMES;

        memcpy(
            &queue->samples[
                dst_frame *
                FLIP_AUDIO_QUEUE_CHANNELS],
            &samples[
                frame *
                FLIP_AUDIO_QUEUE_CHANNELS],
            FLIP_AUDIO_QUEUE_CHANNELS *
                sizeof(int16_t));
    }

    queue->write_frame =
        (queue->write_frame + accepted) %
        FLIP_AUDIO_QUEUE_FRAMES;

    queue->frames += accepted;

    if (queue->frames >
        queue->peak_frames) {

        queue->peak_frames =
            queue->frames;
    }

    if (accepted < frames) {
        queue->dropped_frames +=
            frames - accepted;
    }

    return accepted;
}

size_t flip_audio_queue_read(
    flip_audio_queue_t *queue,
    int16_t *samples,
    size_t max_frames)
{
    if (!queue ||
        !samples ||
        max_frames == 0 ||
        queue->frames == 0) {

        return 0;
    }

    size_t count =
        queue->frames < max_frames
        ? queue->frames
        : max_frames;

    for (size_t frame = 0;
         frame < count;
         ++frame) {

        size_t src_frame =
            (queue->read_frame + frame) %
            FLIP_AUDIO_QUEUE_FRAMES;

        memcpy(
            &samples[
                frame *
                FLIP_AUDIO_QUEUE_CHANNELS],
            &queue->samples[
                src_frame *
                FLIP_AUDIO_QUEUE_CHANNELS],
            FLIP_AUDIO_QUEUE_CHANNELS *
                sizeof(int16_t));
    }

    queue->read_frame =
        (queue->read_frame + count) %
        FLIP_AUDIO_QUEUE_FRAMES;

    queue->frames -= count;

    return count;
}

size_t flip_audio_queue_frames(
    const flip_audio_queue_t *queue)
{
    return queue
        ? queue->frames
        : 0;
}

void flip_audio_queue_reset(
    flip_audio_queue_t *queue)
{
    if (!queue)
        return;

    queue->read_frame = 0;
    queue->write_frame = 0;
    queue->frames = 0;
    queue->peak_frames = 0;
    queue->dropped_frames = 0;
}
