#include "flip_audio_buffer.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CAPTURE_MAX_FRAMES 1024

typedef struct {
    int16_t samples[CAPTURE_MAX_FRAMES * 2];
    size_t frames;
    int fail;
} capture_t;

static int capture_sink(
    const int16_t *samples,
    size_t frames,
    void *userdata)
{
    capture_t *capture =
        (capture_t *)userdata;

    if (capture->fail)
        return -1;

    assert(
        capture->frames + frames <=
        CAPTURE_MAX_FRAMES);

    memcpy(
        &capture->samples[
            capture->frames * 2],
        samples,
        frames * 2 * sizeof(int16_t));

    capture->frames += frames;
    return 0;
}

static void fill_source(
    int16_t *samples,
    size_t frames)
{
    for (size_t i = 0;
         i < frames * 2;
         ++i) {

        samples[i] =
            (int16_t)(i & 0x7fff);
    }
}

static void test_end_frame_flush(void)
{
    flip_audio_buffer_t buffer;
    capture_t capture = {0};
    int16_t source[100 * 2];
    int failed = 0;

    fill_source(
        source,
        100);

    flip_audio_buffer_init(
        &buffer);

    assert(
        flip_audio_buffer_submit(
            &buffer,
            source,
            100,
            capture_sink,
            &capture,
            &failed) == 100);

    assert(failed == 0);
    assert(capture.frames == 0);
    assert(buffer.frames == 100);

    assert(
        flip_audio_buffer_flush(
            &buffer,
            capture_sink,
            &capture) == 0);

    assert(capture.frames == 100);
    assert(buffer.frames == 0);
    assert(
        memcmp(
            source,
            capture.samples,
            sizeof(source)) == 0);
}

static void test_bounded_chunk_flush(void)
{
    flip_audio_buffer_t buffer;
    capture_t capture = {0};
    int16_t source[300 * 2];
    int failed = 0;

    fill_source(
        source,
        300);

    flip_audio_buffer_init(
        &buffer);

    assert(
        flip_audio_buffer_submit(
            &buffer,
            source,
            300,
            capture_sink,
            &capture,
            &failed) == 300);

    assert(failed == 0);
    assert(
        capture.frames ==
        FLIP_AUDIO_BUFFER_FRAMES);
    assert(
        buffer.frames ==
        300 - FLIP_AUDIO_BUFFER_FRAMES);

    assert(
        flip_audio_buffer_flush(
            &buffer,
            capture_sink,
            &capture) == 0);

    assert(capture.frames == 300);
    assert(buffer.frames == 0);
    assert(
        memcmp(
            source,
            capture.samples,
            sizeof(source)) == 0);
}

static void test_sink_failure(void)
{
    flip_audio_buffer_t buffer;
    capture_t capture = {0};
    int16_t source[300 * 2];
    int failed = 0;

    fill_source(
        source,
        300);

    flip_audio_buffer_init(
        &buffer);

    capture.fail = 1;

    size_t accepted =
        flip_audio_buffer_submit(
            &buffer,
            source,
            300,
            capture_sink,
            &capture,
            &failed);

    assert(
        accepted ==
        FLIP_AUDIO_BUFFER_FRAMES);
    assert(failed == 1);
    assert(
        buffer.frames ==
        FLIP_AUDIO_BUFFER_FRAMES);

    flip_audio_buffer_reset(
        &buffer);

    assert(buffer.frames == 0);
}

int main(void)
{
    test_end_frame_flush();
    test_bounded_chunk_flush();
    test_sink_failure();

    puts(
        "flip_audio_buffer tests passed");

    return 0;
}
