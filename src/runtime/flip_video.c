#define _POSIX_C_SOURCE 200809L

#include "flip_video.h"

#include "flip_drm.h"
#include "flip_draw.h"
#include "flip_gpu_client.h"
#include "flip_osd.h"
#include "flip_video_layout.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static flip_drm_t video_drm;
static flip_drm_t video_overlay_drm;
static uint32_t video_overlay_pixels[640u * 480u];
static uint32_t video_osd_pixels[420u * 36u];
static uint32_t video_perf_pixels[190u * 100u];
static uint32_t video_quick_base_pixels[640u * 480u];
static int video_quick_base_valid = 0;
static flip_gpu_client_t video_gpu;

static int video_open = 0;
static int video_gpu_active = 0;
static int video_gpu_osd_active = 0;
static int video_gpu_perf_active = 0;

#define FLIP_GPU_SERVICE_PATH "/mnt/SDCARD/flipos-dev/bin/flip-gpu-service"

/*
 * The shared GPU scanout helper currently reaches LISTENING on RK3566 but
 * the emulator-side client can still miss the Function/DRM handoff and wait
 * for its five-second startup timeout. Keep the implementation available for
 * development, but use the proven direct DRM path for Beta so game launches
 * never pay that timeout.
 */
#define FLIP_VIDEO_GPU_HOST_ENABLED 0

static int perf_overlay_enabled = 0;
static flip_telemetry_snapshot_t perf_overlay_snapshot;

typedef struct {
    uint64_t sum_us;
    uint64_t max_us;
} video_perf_metric_t;

typedef struct {
    uint64_t frames;
    video_perf_metric_t video;
    video_perf_metric_t compose;
    video_perf_metric_t present;
    video_perf_metric_t flip_ioctl;
    video_perf_metric_t flip_wait;
    video_perf_metric_t flip_read;
} video_perf_diag_t;

static video_perf_diag_t video_perf_diag;
static uint64_t video_perf_interval_start_us = 0;

static uint64_t monotonic_us(void)
{
    struct timespec now;

    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0)
        return 0;

    return
        (uint64_t)now.tv_sec * 1000000ULL +
        (uint64_t)now.tv_nsec / 1000ULL;
}

static uint64_t elapsed_us(uint64_t start_us)
{
    uint64_t now_us = monotonic_us();

    if (now_us < start_us)
        return 0;

    return now_us - start_us;
}

static void video_perf_add(
    video_perf_metric_t *metric,
    uint64_t us)
{
    metric->sum_us += us;

    if (us > metric->max_us)
        metric->max_us = us;
}

static uint64_t video_perf_avg(
    const video_perf_metric_t *metric,
    uint64_t frames)
{
    if (!metric || frames == 0)
        return 0;

    return metric->sum_us / frames;
}

static void video_perf_record_frame(
    uint64_t video_us,
    uint64_t compose_us)
{
    video_perf_diag.frames++;

    video_perf_add(&video_perf_diag.video, video_us);
    video_perf_add(&video_perf_diag.compose, compose_us);
    video_perf_add(&video_perf_diag.present, video_drm.last_present_us);
    video_perf_add(&video_perf_diag.flip_ioctl, video_drm.last_flip_ioctl_us);
    video_perf_add(&video_perf_diag.flip_wait, video_drm.last_flip_wait_us);
    video_perf_add(&video_perf_diag.flip_read, video_drm.last_flip_read_us);

    uint64_t now_us = monotonic_us();

    if (video_perf_interval_start_us == 0) {
        video_perf_interval_start_us = now_us;
        return;
    }

    if (now_us - video_perf_interval_start_us < 1000000ULL)
        return;

    fprintf(
        stderr,
        "FLIP_VIDEO_PERF: frames=%llu "
        "video_avg_us=%llu video_max_us=%llu "
        "compose_avg_us=%llu compose_max_us=%llu "
        "present_avg_us=%llu present_max_us=%llu "
        "flip_ioctl_avg_us=%llu flip_ioctl_max_us=%llu "
        "flip_wait_avg_us=%llu flip_wait_max_us=%llu "
        "flip_read_avg_us=%llu flip_read_max_us=%llu\n",
        (unsigned long long)video_perf_diag.frames,
        (unsigned long long)video_perf_avg(
            &video_perf_diag.video,
            video_perf_diag.frames),
        (unsigned long long)video_perf_diag.video.max_us,
        (unsigned long long)video_perf_avg(
            &video_perf_diag.compose,
            video_perf_diag.frames),
        (unsigned long long)video_perf_diag.compose.max_us,
        (unsigned long long)video_perf_avg(
            &video_perf_diag.present,
            video_perf_diag.frames),
        (unsigned long long)video_perf_diag.present.max_us,
        (unsigned long long)video_perf_avg(
            &video_perf_diag.flip_ioctl,
            video_perf_diag.frames),
        (unsigned long long)video_perf_diag.flip_ioctl.max_us,
        (unsigned long long)video_perf_avg(
            &video_perf_diag.flip_wait,
            video_perf_diag.frames),
        (unsigned long long)video_perf_diag.flip_wait.max_us,
        (unsigned long long)video_perf_avg(
            &video_perf_diag.flip_read,
            video_perf_diag.frames),
        (unsigned long long)video_perf_diag.flip_read.max_us);

    memset(
        &video_perf_diag,
        0,
        sizeof(video_perf_diag));

    video_perf_interval_start_us = now_us;
}

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

    memset(
        &video_perf_diag,
        0,
        sizeof(video_perf_diag));
    video_perf_interval_start_us = 0;

    if (FLIP_VIDEO_GPU_HOST_ENABLED &&
        flip_gpu_client_launch(
            &video_gpu,
            FLIP_GPU_SERVICE_PATH,
            1) == 0) {

        memset(
            &video_overlay_drm,
            0,
            sizeof(video_overlay_drm));

        video_overlay_drm.width = 640;
        video_overlay_drm.height = 480;
        video_overlay_drm.pitch =
            640u * sizeof(uint32_t);
        video_overlay_drm.size =
            sizeof(video_overlay_pixels);
        video_overlay_drm.pixels =
            video_overlay_pixels;

        video_gpu_osd_active = 0;
        video_gpu_perf_active = 0;
        video_gpu_active = 1;
        video_open = 1;

        fprintf(
            stderr,
            "VIDEO: opened shared GPU host service=%s\n",
            FLIP_GPU_SERVICE_PATH);

        return 0;
    }

    fprintf(
        stderr,
        "VIDEO: direct CPU/DRM path gpu_host=%s\n",
        FLIP_VIDEO_GPU_HOST_ENABLED ? "enabled-fallback" : "disabled-beta");

    video_gpu_active = 0;

    if (flip_drm_open(
            &video_drm) < 0) {

        return -1;
    }

    video_open = 1;

    fprintf(
        stderr,
        "VIDEO: opened CPU/DRM fallback %ux%u\n",
        video_drm.width,
        video_drm.height);

    return 0;
}

void flip_video_close(void)
{
    if (!video_open)
        return;

    if (video_gpu_active) {
        flip_gpu_client_close(
            &video_gpu);

        video_gpu_active = 0;

        fprintf(
            stderr,
            "VIDEO: closed shared GPU host\n");
    } else {
        flip_drm_close(
            &video_drm);

        fprintf(
            stderr,
            "VIDEO: closed CPU/DRM fallback\n");
    }

    video_open = 0;
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

static void copy_overlay_rect(
    uint32_t *destination,
    unsigned destination_width,
    const flip_drm_t *source,
    int x,
    int y,
    unsigned width,
    unsigned height)
{
    if (!destination ||
        !source ||
        !source->pixels)
        return;

    unsigned source_stride =
        source->pitch /
        sizeof(uint32_t);

    for (unsigned row = 0;
         row < height;
         ++row) {

        memcpy(
            destination +
                row * destination_width,
            source->pixels +
                (y + (int)row) *
                    source_stride +
                x,
            width *
                sizeof(uint32_t));
    }
}

static void update_gpu_osd_overlay(void)
{
    memset(
        video_overlay_pixels,
        0,
        sizeof(video_overlay_pixels));

    int active =
        flip_osd_draw(
            &video_overlay_drm);

    if (active > 0) {
        copy_overlay_rect(
            video_osd_pixels,
            420,
            &video_overlay_drm,
            110,
            406,
            420,
            36);

        int result =
            flip_gpu_client_submit_overlay(
                &video_gpu,
                FLIP_GPU_OVERLAY_OSD,
                110,
                406,
                video_osd_pixels,
                420,
                36,
                420u *
                    sizeof(uint32_t));

        if (result >= 0)
            video_gpu_osd_active = 1;

        return;
    }

    if (video_gpu_osd_active) {
        int result =
            flip_gpu_client_clear_overlay(
                &video_gpu,
                FLIP_GPU_OVERLAY_OSD);

        if (result == 0)
            video_gpu_osd_active = 0;
    }
}

static void update_gpu_perf_overlay(void)
{
    if (!perf_overlay_enabled) {
        if (video_gpu_perf_active) {
            int result =
                flip_gpu_client_clear_overlay(
                    &video_gpu,
                    FLIP_GPU_OVERLAY_PERF);

            if (result == 0)
                video_gpu_perf_active = 0;
        }

        return;
    }

    memset(
        video_overlay_pixels,
        0,
        sizeof(video_overlay_pixels));

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
        video_overlay_drm.pitch /
        sizeof(uint32_t);

    for (int yy = 0;
         yy < box_h;
         ++yy) {

        uint32_t *row =
            video_overlay_drm.pixels +
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
         line <
            FLIP_TELEMETRY_OVERLAY_LINES;
         ++line) {

        fps_draw_text(
            &video_overlay_drm,
            text_x,
            text_y +
                line * line_step,
            lines[line],
            2,
            0x00ffffff);
    }

    copy_overlay_rect(
        video_perf_pixels,
        190,
        &video_overlay_drm,
        box_x,
        box_y,
        box_w,
        box_h);

    int result =
        flip_gpu_client_submit_overlay(
            &video_gpu,
            FLIP_GPU_OVERLAY_PERF,
            box_x,
            box_y,
            video_perf_pixels,
            box_w,
            box_h,
            box_w *
                sizeof(uint32_t));

    if (result >= 0)
        video_gpu_perf_active = 1;
}

static void update_gpu_overlays(void)
{
    update_gpu_osd_overlay();
    update_gpu_perf_overlay();
}


static int quick_menu_copy_from_drm(void)
{
    if (!video_open ||
        video_gpu_active ||
        !video_drm.pixels ||
        video_drm.width != 640 ||
        video_drm.height != 480)
        return -1;

    uint32_t stride =
        video_drm.pitch /
        sizeof(uint32_t);

    for (unsigned y = 0;
         y < 480;
         ++y) {

        memcpy(
            video_quick_base_pixels +
                y * 640u,
            video_drm.pixels +
                y * stride,
            640u * sizeof(uint32_t));
    }

    return 0;
}

static void quick_menu_restore_to_drm(void)
{
    if (!video_quick_base_valid ||
        !video_drm.pixels)
        return;

    uint32_t stride =
        video_drm.pitch /
        sizeof(uint32_t);

    for (unsigned y = 0;
         y < 480;
         ++y) {

        memcpy(
            video_drm.pixels +
                y * stride,
            video_quick_base_pixels +
                y * 640u,
            640u * sizeof(uint32_t));
    }
}

int flip_video_quick_menu_begin(void)
{
    if (quick_menu_copy_from_drm() < 0)
        return -1;

    video_quick_base_valid = 1;
    return 0;
}

int flip_video_quick_menu_present(
    const flip_video_quick_menu_view_t *view)
{
    if (!view ||
        !video_quick_base_valid ||
        !video_open ||
        video_gpu_active)
        return -1;

    quick_menu_restore_to_drm();

    uint32_t stride =
        video_drm.pitch /
        sizeof(uint32_t);

    /*
     * Dim the frozen game frame rather than replacing it. The player keeps
     * visual context while the core itself remains paused.
     */
    for (unsigned y = 0;
         y < 480;
         ++y) {

        uint32_t *row =
            video_drm.pixels +
            y * stride;

        for (unsigned x = 0;
             x < 640;
             ++x) {

            row[x] =
                (row[x] >> 2) &
                0x003f3f3f;
        }
    }

    const uint32_t panel = 0x00181818;
    const uint32_t fg = 0x00ffffff;
    const uint32_t dim = 0x00909090;

    const int panel_x = 92;
    const int panel_y = 34;
    const int panel_w = 456;
    const int panel_h = 412;

    flip_draw_rect(
        &video_drm,
        panel_x,
        panel_y,
        panel_w,
        panel_h,
        panel);

    flip_draw_rect_outline(
        &video_drm,
        panel_x,
        panel_y,
        panel_w,
        panel_h,
        2,
        fg);

    flip_draw_text(
        &video_drm,
        214,
        54,
        "QUICK MENU",
        3,
        fg);

    static const char *labels[] = {
        "RESUME",
        "SAVE STATE",
        "LOAD STATE",
        "VOLUME",
        "BRIGHTNESS",
        "PERFORMANCE HUD",
        "GAMESWITCHER",
        "QUIT GAME"
    };

    const int item_count =
        (int)(sizeof(labels) /
              sizeof(labels[0]));

    for (int i = 0;
         i < item_count;
         ++i) {

        int y =
            104 +
            i * 36;

        int selected =
            i == view->selected;

        if (selected) {
            flip_draw_rect(
                &video_drm,
                118,
                y - 9,
                404,
                29,
                fg);
        }

        flip_draw_text(
            &video_drm,
            136,
            y,
            labels[i],
            2,
            selected
                ? 0x00000000
                : fg);

        char value[32];

        value[0] = '\0';

        if (i == 3) {
            snprintf(
                value,
                sizeof(value),
                "%d",
                view->volume_percent);
        } else if (i == 4) {
            snprintf(
                value,
                sizeof(value),
                "%d",
                view->brightness_percent);
        } else if (i == 5) {
            snprintf(
                value,
                sizeof(value),
                "%s",
                view->performance_enabled
                    ? "ON"
                    : "OFF");
        }

        if (value[0]) {
            int text_x =
                470 -
                (int)strlen(value) * 12;

            flip_draw_text(
                &video_drm,
                text_x,
                y,
                value,
                2,
                selected
                    ? 0x00000000
                    : dim);
        }
    }

    if (view->status &&
        view->status[0]) {

        flip_draw_text(
            &video_drm,
            124,
            402,
            view->status,
            1,
            fg);
    }

    flip_draw_text(
        &video_drm,
        124,
        424,
        "A SELECT   B OR MENU RESUME   LEFT RIGHT ADJUST",
        1,
        dim);

    return flip_drm_present(
        &video_drm);
}

void flip_video_quick_menu_end(void)
{
    if (!video_quick_base_valid)
        return;

    if (video_open &&
        !video_gpu_active) {

        quick_menu_restore_to_drm();

        (void)flip_drm_present(
            &video_drm);
    }

    video_quick_base_valid = 0;
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

    if (video_gpu_active) {
        update_gpu_overlays();

        flip_gpu_format_t gpu_format;

        switch (format) {
            case FLIP_VIDEO_FORMAT_RGB565:
                gpu_format = FLIP_GPU_FORMAT_RGB565;
                break;

            case FLIP_VIDEO_FORMAT_XRGB8888:
                gpu_format = FLIP_GPU_FORMAT_XRGB8888;
                break;

            default:
                return -1;
        }

        uint64_t video_start_us =
            monotonic_us();

        int submit_result =
            flip_gpu_client_submit(
                &video_gpu,
                data,
                width,
                height,
                pitch,
                gpu_format);

        uint64_t video_us =
            elapsed_us(video_start_us);

        video_perf_record_frame(
            video_us,
            0);

        if (submit_result == 1)
            return 0;

        if (submit_result < 0) {
            fprintf(
                stderr,
                "VIDEO: GPU frame submission failed\n");
            return -1;
        }

        return 0;
    }

    uint64_t video_start_us =
        monotonic_us();
    uint64_t compose_start_us =
        video_start_us;

    uint32_t *dst =
        video_drm.pixels;

    uint32_t dst_stride =
        video_drm.pitch /
        sizeof(uint32_t);

    /*
     * Use the largest integer scale that fits the active
     * display. This preserves the proven Game Boy layout
     * exactly (160x144 -> 480x432 at 3x) while allowing
     * larger cores such as GBA (240x160 -> 480x320 at 2x)
     * to reuse the same Video Host safely.
     */
    flip_video_layout_t layout;

    if (flip_video_layout_compute(
            width,
            height,
            video_drm.width,
            video_drm.height,
            &layout) < 0) {

        fprintf(
            stderr,
            "VIDEO: unsupported layout source=%ux%u display=%ux%u\n",
            width,
            height,
            video_drm.width,
            video_drm.height);

        return -1;
    }

    const unsigned scale =
        layout.scale;

    int x0 =
        layout.x;

    int y0 =
        layout.y;

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

    uint64_t compose_us =
        elapsed_us(compose_start_us);

    int present_result =
        flip_drm_present(
            &video_drm);

    uint64_t video_us =
        elapsed_us(video_start_us);

    video_perf_record_frame(
        video_us,
        compose_us);

    return present_result;
}
