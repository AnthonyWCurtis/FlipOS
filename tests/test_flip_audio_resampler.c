#include "flip_audio_resampler.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void test_equal_rate_identity(void)
{
    flip_audio_resampler_t r;
    int16_t in[] = {
        1, 101,
        2, 102,
        3, 103,
        4, 104,
    };
    int16_t out[8] = {0};
    size_t consumed = 0;

    assert(flip_audio_resampler_init(&r, 32000, 32000, 2) == 0);

    size_t produced =
        flip_audio_resampler_process(
            &r,
            in,
            4,
            &consumed,
            out,
            4);

    assert(consumed == 4);
    assert(produced == 4);
    assert(memcmp(in, out, sizeof(in)) == 0);
}

static void test_32768_to_32000_one_second(void)
{
    flip_audio_resampler_t r;
    const size_t frames = 32768;
    int16_t *in = calloc(frames * 2, sizeof(*in));
    int16_t *out = calloc(32010 * 2, sizeof(*out));
    size_t consumed = 0;

    assert(in);
    assert(out);

    for (size_t i = 0; i < frames; ++i) {
        in[i * 2] = (int16_t)(i % 30000);
        in[i * 2 + 1] = (int16_t)(-(int)(i % 30000));
    }

    assert(flip_audio_resampler_init(&r, 32768, 32000, 2) == 0);

    size_t produced =
        flip_audio_resampler_process(
            &r,
            in,
            frames,
            &consumed,
            out,
            32010);

    assert(consumed == frames);
    assert(produced == 32000);

    free(in);
    free(out);
}

static void test_chunk_boundaries_preserve_output(void)
{
    enum {
        FRAMES = 1000,
        OUTPUT_CAPACITY = 1200,
    };

    int16_t in[FRAMES * 2];
    int16_t one_chunk[OUTPUT_CAPACITY * 2];
    int16_t many_chunks[OUTPUT_CAPACITY * 2];

    for (int i = 0; i < FRAMES; ++i) {
        in[i * 2] = (int16_t)(i * 17 - 8000);
        in[i * 2 + 1] = (int16_t)(8000 - i * 13);
    }

    flip_audio_resampler_t a;
    flip_audio_resampler_t b;
    size_t consumed = 0;

    assert(flip_audio_resampler_init(&a, 32768, 32000, 2) == 0);
    assert(flip_audio_resampler_init(&b, 32768, 32000, 2) == 0);

    size_t produced_a =
        flip_audio_resampler_process(
            &a,
            in,
            FRAMES,
            &consumed,
            one_chunk,
            OUTPUT_CAPACITY);

    assert(consumed == FRAMES);

    size_t input_offset = 0;
    size_t produced_b = 0;

    while (input_offset < FRAMES) {
        size_t chunk = (input_offset % 7) + 1;

        if (chunk > FRAMES - input_offset)
            chunk = FRAMES - input_offset;

        consumed = 0;

        size_t produced =
            flip_audio_resampler_process(
                &b,
                in + input_offset * 2,
                chunk,
                &consumed,
                many_chunks + produced_b * 2,
                OUTPUT_CAPACITY - produced_b);

        assert(consumed == chunk);

        input_offset += consumed;
        produced_b += produced;
    }

    assert(produced_a == produced_b);
    assert(
        memcmp(
            one_chunk,
            many_chunks,
            produced_a * 2 * sizeof(int16_t)) == 0);
}

static void test_output_capacity_can_resume_without_loss(void)
{
    flip_audio_resampler_t r;
    int16_t in[] = {
        0, 1000,
        10000, 11000,
        20000, 21000,
    };
    int16_t out[16] = {0};
    size_t total_consumed = 0;
    size_t total_produced = 0;

    assert(flip_audio_resampler_init(&r, 16000, 32000, 2) == 0);

    while (total_consumed < 3) {
        size_t consumed = 0;

        size_t produced =
            flip_audio_resampler_process(
                &r,
                in + total_consumed * 2,
                3 - total_consumed,
                &consumed,
                out + total_produced * 2,
                1);

        assert(produced <= 1);
        assert(consumed > 0 || produced > 0);

        total_consumed += consumed;
        total_produced += produced;
    }

    assert(total_produced == 5);
}

static void test_reset_clears_stream_state(void)
{
    flip_audio_resampler_t r;
    int16_t in[] = {
        10, 20,
        30, 40,
    };
    int16_t out[4];
    size_t consumed = 0;

    assert(flip_audio_resampler_init(&r, 32768, 32000, 2) == 0);

    (void)flip_audio_resampler_process(
        &r,
        in,
        2,
        &consumed,
        out,
        2);

    assert(r.have_previous);

    flip_audio_resampler_reset(&r);

    assert(!r.have_previous);
    assert(r.phase_num == 0);
    assert(r.source_rate == 32768);
    assert(r.output_rate == 32000);
    assert(r.channels == 2);
}

static void test_signed_extrema_are_safe(void)
{
    flip_audio_resampler_t r;
    int16_t in[] = {
        INT16_MIN, INT16_MAX,
        INT16_MAX, INT16_MIN,
        INT16_MIN, INT16_MAX,
    };
    int16_t out[16];
    size_t consumed = 0;

    assert(flip_audio_resampler_init(&r, 16000, 32000, 2) == 0);

    size_t produced =
        flip_audio_resampler_process(
            &r,
            in,
            3,
            &consumed,
            out,
            8);

    assert(consumed == 3);
    assert(produced == 5);
}

int main(void)
{
    test_equal_rate_identity();
    test_32768_to_32000_one_second();
    test_chunk_boundaries_preserve_output();
    test_output_capacity_can_resume_without_loss();
    test_reset_clears_stream_state();
    test_signed_extrema_are_safe();

    puts("flip_audio_resampler tests passed");
    return 0;
}
