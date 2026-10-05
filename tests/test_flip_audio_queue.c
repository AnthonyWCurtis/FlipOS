#include "flip_audio_queue.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static void fill(
    int16_t *samples,
    size_t frames,
    int base)
{
    for (size_t frame = 0;
         frame < frames;
         ++frame) {

        samples[frame * 2] =
            (int16_t)(base + (int)frame);

        samples[frame * 2 + 1] =
            (int16_t)(-base - (int)frame);
    }
}

static void test_fifo_and_wrap(void)
{
    flip_audio_queue_t queue;
    flip_audio_queue_init(&queue);

    int16_t a[3000 * 2];
    int16_t b[2500 * 2];
    int16_t out[4096 * 2];

    fill(a, 3000, 100);
    fill(b, 2500, 5000);

    assert(
        flip_audio_queue_write(
            &queue,
            a,
            3000) == 3000);

    assert(
        flip_audio_queue_read(
            &queue,
            out,
            2500) == 2500);

    for (size_t i = 0; i < 2500; ++i) {
        assert(out[i * 2] == a[i * 2]);
        assert(out[i * 2 + 1] == a[i * 2 + 1]);
    }

    assert(
        flip_audio_queue_write(
            &queue,
            b,
            2500) == 2500);

    assert(
        flip_audio_queue_frames(
            &queue) == 3000);

    assert(
        flip_audio_queue_read(
            &queue,
            out,
            3000) == 3000);

    for (size_t i = 0; i < 500; ++i) {
        assert(out[i * 2] == a[(2500 + i) * 2]);
        assert(out[i * 2 + 1] == a[(2500 + i) * 2 + 1]);
    }

    for (size_t i = 0; i < 2500; ++i) {
        assert(out[(500 + i) * 2] == b[i * 2]);
        assert(out[(500 + i) * 2 + 1] == b[i * 2 + 1]);
    }
}

static void test_bounded_drop_newest(void)
{
    flip_audio_queue_t queue;
    flip_audio_queue_init(&queue);

    int16_t samples[
        (FLIP_AUDIO_QUEUE_FRAMES + 100) * 2];

    fill(
        samples,
        FLIP_AUDIO_QUEUE_FRAMES + 100,
        10);

    assert(
        flip_audio_queue_write(
            &queue,
            samples,
            FLIP_AUDIO_QUEUE_FRAMES + 100) ==
        FLIP_AUDIO_QUEUE_FRAMES);

    assert(
        queue.frames ==
        FLIP_AUDIO_QUEUE_FRAMES);

    assert(
        queue.peak_frames ==
        FLIP_AUDIO_QUEUE_FRAMES);

    assert(
        queue.dropped_frames ==
        100);
}

int main(void)
{
    test_fifo_and_wrap();
    test_bounded_drop_newest();

    puts("flip_audio_queue tests passed");
    return 0;
}
