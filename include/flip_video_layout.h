#ifndef FLIP_VIDEO_LAYOUT_H
#define FLIP_VIDEO_LAYOUT_H

typedef struct {
    unsigned scale;
    unsigned output_width;
    unsigned output_height;
    int x;
    int y;
} flip_video_layout_t;

int flip_video_layout_compute(
    unsigned source_width,
    unsigned source_height,
    unsigned display_width,
    unsigned display_height,
    flip_video_layout_t *layout);

#endif
