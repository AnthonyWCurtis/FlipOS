#include "flip_video.h"

#include "flip_drm.h"
#include "flip_osd.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static flip_drm_t video_drm;

static int video_open = 0;

static int perf_overlay_enabled = 0;
static flip_telemetry_snapshot_t perf_overlay_snapshot;

static const uint8_t *fps_glyph(
    char c)
{
    static const uint8_t space[7] = {
        0x00,0x00,0x00,0x00,0x00,0x00,0x00
    };

    static const uint8_t zero[7] = {
        0x0e,0x11,0x13,0x15,0x19,0x11,0x0e
    };

    static const uint8_t one[7] = {
        0x04,0x0c,0x04,0x04,0x04,0x04,0x0e
    };

    static const uint8_t two[7] = {
        0x0e,0x11,0x01,0x02,0x04,0x08,0x1f
    };

    static const uint8_t three[7] = {
        0x1e,0x01,0x01,0x0e,0x01,0x01,0x1e
    };

    static const uint8_t four[7] = {
        0x02,0x06,0x0a,0x12,0x1f,0x02,0x02
    };

    static const uint8_t five[7] = {
        0x1f,0x10,0x10,0x1e,0x01,0x01,0x1e
    };

    static const uint8_t six[7] = {
        0x0e,0x10,0x10,0x1e,0x11,0x11,0x0e
    };

    static const uint8_t seven[7] = {
        0x1f,0x01,0x02,0x04,0x08,0x08,0x08
    };

    static const uint8_t eight[7] = {
        0x0e,0x11,0x11,0x0e,0x11,0x11,0x0e
    };

    static const uint8_t nine[7] = {
        0x0e,0x11,0x11,0x0f,0x01,0x01,0x0e
    };

    static const uint8_t a[7] = {
        0x0e,0x11,0x11,0x1f,0x11,0x11,0x11
    };

    static const uint8_t b[7] = {
        0x1e,0x11,0x11,0x1e,0x11,0x11,0x1e
    };

    static const uint8_t c_glyph[7] = {
        0x0e,0x11,0x10,0x10,0x10,0x11,0x0e
    };

    static const uint8_t f[7] = {
        0x1f,0x10,0x10,0x1e,0x10,0x10,0x10
    };

    static const uint8_t g[7] = {
        0x0e,0x11,0x10,0x17,0x11,0x11,0x0f
    };

    static const uint8_t m[7] = {
        0x11,0x1b,0x15,0x15,0x11,0x11,0x11
    };

    static const uint8_t p[7] = {
        0x1e,0x11,0x11,0x1e,0x10,0x10,0x10
    };

    static const uint8_t s[7] = {
        0x0f,0x10,0x10,0x0e,0x01,0x01,0x1e
    };

    static const uint8_t t[7] = {
        0x1f,0x04,0x04,0x04,0x04,0x04,0x04
    };

    static const uint8_t u[7] = {
        0x11,0x11,0x11,0x11,0x11,0x11,0x0e
    };

    static const uint8_t dot[7] = {
        0x00,0x00,0x00,0x00,0x00,0x06,0x06
    };

    static const uint8_t dash[7] = {
        0x00,0x00,0x00,0x1f,0x00,0x00,0x00
    };

    static const uint8_t percent[7] = {
        0x19,0x19,0x02,0x04,0x08,0x13,0x13
    };

    switch (c) {
        case '0': return zero;
        case '1': return one;
        case '2': return two;
        case '3': return three;
        case '4': return four;
        case '5': return five;
        case '6': return six;
        case '7': return seven;
        case '8': return eight;
        case '9': return nine;
        case 'A': return a;
        case 'B': return b;
        case 'C': return c_glyph;
        case 'F': return f;
        case 'G': return g;
        case 'M': return m;
        case 'P': return p;
        case 'S': return s;
        case 'T': return t;
        case 'U': return u;
        case '.': return dot;
        case '-': return dash;
        case '%': return percent;
        default:  return space;
    }
}

static void fps_draw_char(
    flip_drm_t *drm,
    int x,
    int y,
    char c,
    int scale,
    uint32_t colour)
{
    const uint8_t *glyph =
        fps_glyph(c);

    uint32_t stride =
        drm->pitch /
        sizeof(uint32_t);

    for (int gy = 0;
         gy < 7;
         ++gy) {

        for (int gx = 0;
             gx < 5;
             ++gx) {

            if (!(glyph[gy] &
                  (1u << (4 - gx)))) {

                continue;
            }

            for (int sy = 0;
                 sy < scale;
                 ++sy) {

                uint32_t *row =
                    drm->pixels +
                    (y +
                     gy * scale +
                     sy) *
                    stride +
                    x +
                    gx * scale;

                for (int sx = 0;
                     sx < scale;
                     ++sx) {

                    row[sx] =
                        colour;
                }
            }
        }
    }
}

static void fps_draw_text(
    flip_drm_t *drm,
    int x,
    int y,
    const char *text,
    int scale,
    uint32_t colour)
{
    while (text &&
           *text) {

        fps_draw_char(
            drm,
            x,
            y,
            *text,
            scale,
            colour);

        x +=
            6 * scale;

        ++text;
    }
}

int flip_video_open(void)
{
    if (video_open)
        return 0;

    if (flip_drm_open(
            &video_drm) < 0) {

        return -1;
    }

    video_open = 1;

    fprintf(
        stderr,
        "VIDEO: opened %ux%u\n",
        video_drm.width,
        video_drm.height);

    return 0;
}

void flip_video_close(void)
{
    if (!video_open)
        return;

    flip_drm_close(
        &video_drm);

    video_open = 0;

    fprintf(
        stderr,
        "VIDEO: closed\n");
}

void flip_video_set_fps_overlay(
    int enabled,
    double fps)
{
    perf_overlay_enabled =
        enabled ? 1 : 0;

    perf_overlay_snapshot.fps = fps;
    perf_overlay_snapshot.fps_valid = 1;
}

void flip_video_set_perf_overlay(
    int enabled,
    const flip_telemetry_snapshot_t *snapshot)
{
    perf_overlay_enabled =
        enabled ? 1 : 0;

    if (snapshot) {
        perf_overlay_snapshot =
            *snapshot;
    } else {
        memset(
            &perf_overlay_snapshot,
            0,
            sizeof(perf_overlay_snapshot));
    }
}

int flip_video_submit(
    const void *data,
    unsigned width,
    unsigned height,
    size_t pitch,
    flip_video_format_t format)
{
    if (!video_open ||
        !data ||
        width == 0 ||
        height == 0) {

        return -1;
    }

    uint32_t *dst =
        video_drm.pixels;

    uint32_t dst_stride =
        video_drm.pitch /
        sizeof(uint32_t);

    /*
     * Phase 4A1 preserves the proven Game Boy presentation
     * exactly:
     *
     *   160x144 -> 480x432
     *
     * using 3x integer scaling, centred in 640x480.
     *
     * Scaling policy becomes configurable later. This phase
     * only moves ownership out of Flip_GB.
     */
    const unsigned scale = 3;

    unsigned out_w =
        width * scale;

    unsigned out_h =
        height * scale;

    int x0 =
        ((int)video_drm.width -
         (int)out_w) /
        2;

    int y0 =
        ((int)video_drm.height -
         (int)out_h) /
        2;

    memset(
        video_drm.pixels,
        0,
        (size_t)video_drm.size);

    if (format ==
        FLIP_VIDEO_FORMAT_RGB565) {

        const uint8_t *src_bytes =
            (const uint8_t *)data;

        for (unsigned sy = 0;
             sy < height;
             ++sy) {

            const uint16_t *src =
                (const uint16_t *)
                (src_bytes +
                 sy * pitch);

            for (unsigned sx = 0;
                 sx < width;
                 ++sx) {

                uint16_t p =
                    src[sx];

                uint32_t r =
                    (p >> 11) &
                    0x1f;

                uint32_t g_value =
                    (p >> 5) &
                    0x3f;

                uint32_t b_value =
                    p &
                    0x1f;

                r =
                    (r << 3) |
                    (r >> 2);

                g_value =
                    (g_value << 2) |
                    (g_value >> 4);

                b_value =
                    (b_value << 3) |
                    (b_value >> 2);

                uint32_t rgb =
                    (r << 16) |
                    (g_value << 8) |
                    b_value;

                int dx =
                    x0 +
                    (int)sx *
                    (int)scale;

                int dy =
                    y0 +
                    (int)sy *
                    (int)scale;

                for (unsigned yy = 0;
                     yy < scale;
                     ++yy) {

                    uint32_t *row =
                        &dst[
                            (dy +
                             (int)yy) *
                            dst_stride +
                            dx];

                    for (unsigned xx = 0;
                         xx < scale;
                         ++xx) {

                        row[xx] =
                            rgb;
                    }
                }
            }
        }

    } else if (
        format ==
        FLIP_VIDEO_FORMAT_XRGB8888) {

        const uint8_t *src_bytes =
            (const uint8_t *)data;

        for (unsigned sy = 0;
             sy < height;
             ++sy) {

            const uint32_t *src =
                (const uint32_t *)
                (src_bytes +
                 sy * pitch);

            for (unsigned sx = 0;
                 sx < width;
                 ++sx) {

                uint32_t rgb =
                    src[sx] &
                    0x00ffffff;

                int dx =
                    x0 +
                    (int)sx *
                    (int)scale;

                int dy =
                    y0 +
                    (int)sy *
                    (int)scale;

                for (unsigned yy = 0;
                     yy < scale;
                     ++yy) {

                    uint32_t *row =
                        &dst[
                            (dy +
                             (int)yy) *
                            dst_stride +
                            dx];

                    for (unsigned xx = 0;
                         xx < scale;
                         ++xx) {

                        row[xx] =
                            rgb;
                    }
                }
            }
        }

    } else {

        return -1;
    }

    if (perf_overlay_enabled) {
        char lines[
            FLIP_TELEMETRY_OVERLAY_LINES]
            [FLIP_TELEMETRY_OVERLAY_LINE_LENGTH];

        flip_telemetry_format_overlay(
            &perf_overlay_snapshot,
            lines);

        const int box_x = 8;
        const int box_y = 8;
        const int box_w = 190;
        const int box_h = 100;
        const int text_x = box_x + 8;
        const int text_y = box_y + 6;
        const int line_step = 18;

        uint32_t stride =
            video_drm.pitch /
            sizeof(uint32_t);

        for (int yy = 0;
             yy < box_h;
             ++yy) {

            uint32_t *row =
                video_drm.pixels +
                (box_y + yy) *
                stride +
                box_x;

            for (int xx = 0;
                 xx < box_w;
                 ++xx) {

                row[xx] =
                    0x00000000;
            }
        }

        for (int line = 0;
             line < FLIP_TELEMETRY_OVERLAY_LINES;
             ++line) {

            fps_draw_text(
                &video_drm,
                text_x,
                text_y +
                    line *
                    line_step,
                lines[line],
                2,
                0x00ffffff);
        }
    }

    /*
     * Global OSD is always the final composition layer
     * before presentation.
     */
    flip_osd_draw(
        &video_drm);

    return flip_drm_present(
        &video_drm);
}
