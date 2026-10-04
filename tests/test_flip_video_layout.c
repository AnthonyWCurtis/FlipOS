#include "flip_video_layout.h"

#include <assert.h>
#include <stdio.h>

static void test_game_boy_layout(void)
{
    flip_video_layout_t layout;

    assert(flip_video_layout_compute(
        160, 144, 640, 480, &layout) == 0);

    assert(layout.scale == 3);
    assert(layout.output_width == 480);
    assert(layout.output_height == 432);
    assert(layout.x == 80);
    assert(layout.y == 24);
}

static void test_game_boy_advance_layout(void)
{
    flip_video_layout_t layout;

    assert(flip_video_layout_compute(
        240, 160, 640, 480, &layout) == 0);

    assert(layout.scale == 2);
    assert(layout.output_width == 480);
    assert(layout.output_height == 320);
    assert(layout.x == 80);
    assert(layout.y == 80);
}

static void test_native_layout(void)
{
    flip_video_layout_t layout;

    assert(flip_video_layout_compute(
        640, 480, 640, 480, &layout) == 0);

    assert(layout.scale == 1);
    assert(layout.x == 0);
    assert(layout.y == 0);
}

static void test_invalid_or_oversized_layout(void)
{
    flip_video_layout_t layout;

    assert(flip_video_layout_compute(
        0, 160, 640, 480, &layout) < 0);

    assert(flip_video_layout_compute(
        800, 600, 640, 480, &layout) < 0);

    assert(flip_video_layout_compute(
        240, 160, 640, 480, NULL) < 0);
}

int main(void)
{
    test_game_boy_layout();
    test_game_boy_advance_layout();
    test_native_layout();
    test_invalid_or_oversized_layout();

    puts("flip_video_layout tests passed");
    return 0;
}
