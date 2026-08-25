#include "flip_video.h"

#include "flip_drm.h"
#include "flip_osd.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static flip_drm_t video_drm;

static int video_open = 0;

static int fps_overlay_enabled = 0;
static double fps_overlay_value = 0.0;

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

    static const uint8_t f[7] = {
        0x1f,0x10,0x10,0x1e,0x10,0x10,0x10
    };

    static const uint8_t p[7] = {
        0x1e,0x11,0x11,0x1e,0x10,0x10,0x10
    };

    static const uint8_t s[7] = {
        0x0f,0x10,0x10,0x0e,0x01,0x01,0x1e
    };

    static const uint8_t dot[7] = {
        0x00,0x00,0x00,0x00,0x00,0x06,0x06
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
        case 'F': return f;
        case 'P': return p;
        case 'S': return s;
        case '.': return dot;
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
    fps_overlay_enabled =
        enabled ? 1 : 0;

    fps_overlay_value =
        fps;
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

                uint32_t g =
                    (p >> 5) &
                    0x3f;

                uint32_t b =
                    p &
                    0x1f;

                r =
                    (r << 3) |
                    (r >> 2);

                g =
                    (g << 2) |
                    (g >> 4);

                b =
                    (b << 3) |
                    (b >> 2);

                uint32_t rgb =
                    (r << 16) |
                    (g << 8) |
                    b;

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

    if (fps_overlay_enabled) {

        char fps_text[32];

        snprintf(
            fps_text,
            sizeof(fps_text),
            "FPS %.1f",
            fps_overlay_value);

        const int box_x = 90;
        const int box_y = 36;
        const int box_w = 140;
        const int box_h = 32;

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

        fps_draw_text(
            &video_drm,
            box_x + 8,
            box_y + 8,
            fps_text,
            2,
            0x00ffffff);
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
