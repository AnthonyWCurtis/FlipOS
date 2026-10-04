#include "flip_video_layout.h"

int flip_video_layout_compute(
    unsigned source_width,
    unsigned source_height,
    unsigned display_width,
    unsigned display_height,
    flip_video_layout_t *layout)
{
    if (!layout ||
        source_width == 0 ||
        source_height == 0 ||
        display_width == 0 ||
        display_height == 0) {
        return -1;
    }

    unsigned scale_x = display_width / source_width;
    unsigned scale_y = display_height / source_height;
    unsigned scale = scale_x < scale_y ? scale_x : scale_y;

    if (scale == 0)
        return -1;

    layout->scale = scale;
    layout->output_width = source_width * scale;
    layout->output_height = source_height * scale;
    layout->x = ((int)display_width - (int)layout->output_width) / 2;
    layout->y = ((int)display_height - (int)layout->output_height) / 2;

    return 0;
}
