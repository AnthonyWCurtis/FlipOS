#define _GNU_SOURCE

#include "flip_video.h"
#include "flip_settings.h"
#include "flip_power.h"
#include "flip_audio.h"
#include "flip_input.h"
#include "flip_controls.h"
#include "flip_timing.h"
#include "flip_hotkeys.h"

#include "libretro/libretro.h"

#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static flip_input_state_t physical_input;
static flip_controls_t controls;
static flip_controls_state_t control_state;
static flip_timing_t timing;

static enum retro_pixel_format pixel_format =
    RETRO_PIXEL_FORMAT_RGB565;

static const char *rom_path = NULL;

static int fps_overlay_enabled = 0;
static double fps_overlay_value = 0.0;

#define FLIPGB_EXIT_SWITCHER 20


#define GB_STATE_DIR \
    "/mnt/SDCARD/flipos-dev/states/gb"

static uint64_t state_path_hash(
    const char *text)
{
    uint64_t hash =
        UINT64_C(14695981039346656037);

    while (text && *text) {
        hash ^=
            (unsigned char)*text++;

        hash *=
            UINT64_C(1099511628211);
    }

    return hash;
}

static int ensure_state_directory(void)
{
    if (mkdir(
            "/mnt/SDCARD/flipos-dev/states",
            0777) < 0 &&
        errno != EEXIST)
        return -1;

    if (mkdir(
            GB_STATE_DIR,
            0777) < 0 &&
        errno != EEXIST)
        return -1;

    return 0;
}

static int build_state_path(
    char *path,
    size_t path_size)
{
    if (!path ||
        path_size == 0 ||
        !rom_path)
        return -1;

    uint64_t hash =
        state_path_hash(
            rom_path);

    snprintf(
        path,
        path_size,
        GB_STATE_DIR "/%016llx.state",
        (unsigned long long)hash);

    return 0;
}

static int save_resume_state(void)
{
    size_t size =
        retro_serialize_size();

    if (size == 0) {
        fprintf(
            stderr,
            "FLIPGB_STATE: serialize_size=0\n");

        return -1;
    }

    void *data =
        malloc(size);

    if (!data) {
        fprintf(
            stderr,
            "FLIPGB_STATE: malloc failed size=%zu\n",
            size);

        return -1;
    }

    if (!retro_serialize(
            data,
            size)) {

        fprintf(
            stderr,
            "FLIPGB_STATE: serialize failed\n");

        free(data);
        return -1;
    }

    if (ensure_state_directory() < 0) {

        fprintf(
            stderr,
            "FLIPGB_STATE: mkdir failed: %s\n",
            strerror(errno));

        free(data);
        return -1;
    }

    char path[512];
    char tmp[544];

    if (build_state_path(
            path,
            sizeof(path)) < 0) {

        free(data);
        return -1;
    }

    snprintf(
        tmp,
        sizeof(tmp),
        "%s.tmp",
        path);

    FILE *f =
        fopen(
            tmp,
            "wb");

    if (!f) {
        fprintf(
            stderr,
            "FLIPGB_STATE: fopen failed path=%s error=%s\n",
            tmp,
            strerror(errno));

        free(data);
        return -1;
    }

    size_t written =
        fwrite(
            data,
            1,
            size,
            f);

    int ok =
        written == size;

    if (ok) {
        fflush(f);

        if (fsync(
                fileno(f)) < 0)
            ok = 0;
    }

    fclose(f);
    free(data);

    if (!ok) {
        fprintf(
            stderr,
            "FLIPGB_STATE: write failed path=%s\n",
            tmp);

        unlink(tmp);
        return -1;
    }

    if (rename(
            tmp,
            path) < 0) {

        fprintf(
            stderr,
            "FLIPGB_STATE: rename failed: %s\n",
            strerror(errno));

        unlink(tmp);
        return -1;
    }

    fprintf(
        stderr,
        "FLIPGB_STATE: SAVE_OK path=%s size=%zu\n",
        path,
        size);

    return 0;
}

static int load_resume_state(void)
{
    char path[512];

    if (build_state_path(
            path,
            sizeof(path)) < 0)
        return 0;

    FILE *f =
        fopen(
            path,
            "rb");

    if (!f) {

        if (errno == ENOENT) {
            fprintf(
                stderr,
                "FLIPGB_STATE: no resume state\n");

            return 0;
        }

        fprintf(
            stderr,
            "FLIPGB_STATE: open failed path=%s error=%s\n",
            path,
            strerror(errno));

        return -1;
    }

    size_t expected =
        retro_serialize_size();

    if (expected == 0) {
        fclose(f);
        return -1;
    }

    if (fseek(
            f,
            0,
            SEEK_END) != 0) {

        fclose(f);
        return -1;
    }

    long file_size =
        ftell(f);

    if (file_size < 0 ||
        (size_t)file_size != expected) {

        fprintf(
            stderr,
            "FLIPGB_STATE: size mismatch file=%ld expected=%zu\n",
            file_size,
            expected);

        fclose(f);
        return -1;
    }

    rewind(f);

    void *data =
        malloc(expected);

    if (!data) {
        fclose(f);
        return -1;
    }

    size_t got =
        fread(
            data,
            1,
            expected,
            f);

    fclose(f);

    if (got != expected) {
        fprintf(
            stderr,
            "FLIPGB_STATE: read failed got=%zu expected=%zu\n",
            got,
            expected);

        free(data);
        return -1;
    }

    int ok =
        retro_unserialize(
            data,
            expected);

    free(data);

    if (!ok) {
        fprintf(
            stderr,
            "FLIPGB_STATE: RESTORE_FAILED path=%s\n",
            path);

        return -1;
    }

    fprintf(
        stderr,
        "FLIPGB_STATE: RESTORE_OK path=%s size=%zu\n",
        path,
        expected);

    return 1;
}




static bool env_cb(unsigned cmd, void *data)
{
    switch (cmd) {

        case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
        {
            enum retro_pixel_format fmt =
                *(const enum retro_pixel_format *)data;

            if (fmt == RETRO_PIXEL_FORMAT_RGB565 ||
                fmt == RETRO_PIXEL_FORMAT_XRGB8888) {

                pixel_format = fmt;

                fprintf(
                    stderr,
                    "FLIPGB: pixel_format=%d\n",
                    (int)fmt);

                return true;
            }

            fprintf(
                stderr,
                "FLIPGB: unsupported pixel format %d\n",
                (int)fmt);

            return false;
        }

        case RETRO_ENVIRONMENT_GET_CAN_DUPE:
        {
            bool *can_dupe = (bool *)data;
            *can_dupe = true;
            return true;
        }

        case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
        {
            const char **dir = (const char **)data;

            *dir =
                "/mnt/SDCARD/flipos-dev/system/gb";

            return true;
        }

        case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
        {
            const char **dir = (const char **)data;

            *dir =
                "/mnt/SDCARD/flipos-dev/saves/gb";

            return true;
        }

        case RETRO_ENVIRONMENT_GET_VARIABLE:
        {
            struct retro_variable *var =
                (struct retro_variable *)data;

            /*
             * No custom core options for first boot.
             * Gambatte will use defaults.
             */
            var->value = NULL;
            return true;
        }

        case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
        {
            bool *updated = (bool *)data;
            *updated = false;
            return true;
        }

        case RETRO_ENVIRONMENT_SET_VARIABLES:
        case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
        case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
        case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
            return true;

        default:
            return false;
    }
}


static void video_cb(
    const void *data,
    unsigned width,
    unsigned height,
    size_t pitch)
{
    if (!data)
        return;

    flip_video_format_t format;

    if (pixel_format ==
        RETRO_PIXEL_FORMAT_RGB565) {

        format =
            FLIP_VIDEO_FORMAT_RGB565;

    } else if (
        pixel_format ==
        RETRO_PIXEL_FORMAT_XRGB8888) {

        format =
            FLIP_VIDEO_FORMAT_XRGB8888;

    } else {

        fprintf(
            stderr,
            "FLIPGB: unsupported active video format=%d\n",
            (int)pixel_format);

        return;
    }

    /*
     * Flip_GB now supplies raw emulated video only.
     *
     * Scaling, framebuffer ownership, OSD composition and
     * physical presentation belong to the FlipOS video host.
     */
    if (flip_video_submit(
            data,
            width,
            height,
            pitch,
            format) < 0) {

        fprintf(
            stderr,
            "FLIPGB: video submit failed\n");
    }
}


static void audio_sample_cb(
    int16_t left,
    int16_t right)
{
    int16_t frame[2];

    frame[0] = left;
    frame[1] = right;

    flip_audio_write(
        frame,
        1);
}

static size_t audio_batch_cb(
    const int16_t *data,
    size_t frames)
{
    return flip_audio_write(
        data,
        frames);
}


static void input_poll_cb(void)
{
    /*
     * Hardware polling is owned by the FlipOS host loop.
     *
     * Gambatte may request an input poll at any point during
     * retro_run(), but it reads the state already captured
     * by the host for this frame.
     */
}

static int16_t input_state_cb(
    unsigned port,
    unsigned device,
    unsigned index,
    unsigned id)
{
    (void)index;

    if (port != 0)
        return 0;

    if (device != RETRO_DEVICE_JOYPAD)
        return 0;

    switch (id) {

        case RETRO_DEVICE_ID_JOYPAD_UP:
            return control_state.game.down[
                FLIP_GAME_UP];

        case RETRO_DEVICE_ID_JOYPAD_DOWN:
            return control_state.game.down[
                FLIP_GAME_DOWN];

        case RETRO_DEVICE_ID_JOYPAD_LEFT:
            return control_state.game.down[
                FLIP_GAME_LEFT];

        case RETRO_DEVICE_ID_JOYPAD_RIGHT:
            return control_state.game.down[
                FLIP_GAME_RIGHT];

        case RETRO_DEVICE_ID_JOYPAD_A:
            return control_state.game.down[
                FLIP_GAME_A];

        case RETRO_DEVICE_ID_JOYPAD_B:
            return control_state.game.down[
                FLIP_GAME_B];

        case RETRO_DEVICE_ID_JOYPAD_START:
            return control_state.game.down[
                FLIP_GAME_START];

        case RETRO_DEVICE_ID_JOYPAD_SELECT:
            return control_state.game.down[
                FLIP_GAME_SELECT];

        default:
            return 0;
    }
}

static void unload_rom(
    struct retro_game_info *game)
{
    retro_unload_game();

    if (game->data) {
        free((void *)game->data);
        game->data = NULL;
    }
}

static int load_rom(
    const char *path,
    struct retro_game_info *game)
{
    FILE *f =
        fopen(path, "rb");

    if (!f) {
        perror("FLIPGB: fopen ROM");
        return -1;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -1;
    }

    long len =
        ftell(f);

    if (len <= 0) {
        fclose(f);
        return -1;
    }

    rewind(f);

    void *data =
        malloc((size_t)len);

    if (!data) {
        fclose(f);
        return -1;
    }

    size_t got =
        fread(
            data,
            1,
            (size_t)len,
            f);

    fclose(f);

    if (got != (size_t)len) {
        free(data);
        return -1;
    }

    memset(
        game,
        0,
        sizeof(*game));

    game->path = path;
    game->data = data;
    game->size = (size_t)len;
    game->meta = NULL;

    return 0;
}

int main(
    int argc,
    char **argv)
{
    int flipgb_exit_code = 0;

    if (argc < 2) {
        fprintf(
            stderr,
            "Usage: %s game.gb\n",
            argv[0]);

        return 1;
    }

    rom_path = argv[1];

    fprintf(
        stderr,
        "FLIPGB: rom=%s\n",
        rom_path);

    memset(
        &physical_input,
        0,
        sizeof(physical_input));

    memset(
        &control_state,
        0,
        sizeof(control_state));

    flip_controls_init(
        &controls);

    if (flip_video_open() < 0) {
        fprintf(
            stderr,
            "FLIPGB: DRM init failed\n");

        return 2;
    }

    if (flip_input_open() < 0) {
        fprintf(
            stderr,
            "FLIPGB: input init failed\n");

        flip_video_close();
        return 3;
    }

    retro_set_environment(
        env_cb);

    retro_set_video_refresh(
        video_cb);

    retro_set_audio_sample(
        audio_sample_cb);

    retro_set_audio_sample_batch(
        audio_batch_cb);

    retro_set_input_poll(
        input_poll_cb);

    retro_set_input_state(
        input_state_cb);

    if (flip_audio_open() < 0) {
        fprintf(
            stderr,
            "FLIPGB: audio initialization failed; "
            "continuing silent\n");
    }

    retro_init();

    struct retro_system_info info;
    memset(
        &info,
        0,
        sizeof(info));

    retro_get_system_info(
        &info);

    fprintf(
        stderr,
        "FLIPGB: core=%s version=%s extensions=%s\n",
        info.library_name
            ? info.library_name
            : "?",
        info.library_version
            ? info.library_version
            : "?",
        info.valid_extensions
            ? info.valid_extensions
            : "?");

    struct retro_game_info game;

    if (load_rom(
            rom_path,
            &game) < 0) {

        fprintf(
            stderr,
            "FLIPGB: failed to read ROM\n");

        retro_deinit();
        flip_input_close();
        flip_video_close();

        return 4;
    }

    if (!retro_load_game(&game)) {

        fprintf(
            stderr,
            "FLIPGB: retro_load_game failed\n");

        free((void *)game.data);

        retro_deinit();
        flip_input_close();
        flip_video_close();

        return 5;
    }

    fprintf(
        stderr,
        "FLIPGB: attempting resume\n");

    int resume_result =
        load_resume_state();

    if (resume_result > 0) {

        fprintf(
            stderr,
            "FLIPGB: resumed saved session\n");

    } else if (
        resume_result < 0) {

        fprintf(
            stderr,
            "FLIPGB: resume unavailable; continuing normally\n");
    }


    struct retro_system_av_info av;
    memset(
        &av,
        0,
        sizeof(av));

    retro_get_system_av_info(
        &av);

    fprintf(
        stderr,
        "FLIPGB: video base=%ux%u max=%ux%u fps=%.3f audio=%.1f\n",
        av.geometry.base_width,
        av.geometry.base_height,
        av.geometry.max_width,
        av.geometry.max_height,
        av.timing.fps,
        av.timing.sample_rate);

    /*
     * Libretro cores report their desired frame rate, while
     * the shared FlipOS Timing Host owns the actual monotonic
     * pacing schedule. The GB-specific fallback remains here.
     */
    double fps = av.timing.fps;

    if (fps < 1.0)
        fps = 59.7275;

    if (flip_timing_init(
            &timing,
            fps) < 0) {

        fprintf(
            stderr,
            "FLIPGB: timing initialization failed\n");

        unload_rom(&game);
        retro_deinit();
        flip_audio_close();
        flip_input_close();
        flip_video_close();

        return 6;
    }

    fprintf(
        stderr,
        "FLIPGB: frame pacing fps=%.6f period_ns=%lld\n",
        fps,
        timing.frame_period_ns);

    while (1) {

        /*
         * FlipOS owns physical hardware polling.
         *
         * Take one coherent input snapshot per emulated frame.
         * Gambatte's input_poll_cb() is deliberately passive
         * and input_state_cb() reads this snapshot.
         */
        if (flip_input_poll(
                &physical_input,
                0) < 0) {

            fprintf(
                stderr,
                "FLIPGB: input poll failed\n");

            break;
        }

        if (flip_controls_update(
                &controls,
                &physical_input,
                &control_state) < 0) {

            fprintf(
                stderr,
                "FLIPGB: controls update failed\n");

            break;
        }

        retro_run();

        /*
         * FlipOS logical system actions.
         *
         * Physical button/chord interpretation belongs to
         * flip_controls. Flip_GB only reacts to the resulting
         * platform-neutral host actions.
         */
        if (control_state.system.lid_changed) {

            fprintf(
                stderr,
                "FLIPGB_LID %s\n",
                control_state.system.lid_closed
                    ? "CLOSED"
                    : "OPEN");
        }

        if (control_state.system.suspend_requested) {

            fprintf(
                stderr,
                "FLIPGB_LID suspend requested\n");

            if (save_resume_state() < 0) {

                fprintf(
                    stderr,
                    "FLIPGB_LID suspend save failed\n");

            } else {

                flipgb_exit_code =
                    FLIPGB_EXIT_SWITCHER;

                break;
            }
        }

        if (control_state.system.fps_toggle_requested) {

            fps_overlay_enabled =
                !fps_overlay_enabled;

            flip_video_set_fps_overlay(
                fps_overlay_enabled,
                fps_overlay_value);

            fprintf(
                stderr,
                "FLIPGB: FPS overlay %s\n",
                fps_overlay_enabled
                    ? "ON"
                    : "OFF");
        }

        if (control_state.system.gameswitcher_requested) {

            fprintf(
                stderr,
                "FLIPGB: GameSwitcher requested\n");

            if (save_resume_state() < 0) {

                fprintf(
                    stderr,
                    "FLIPGB_STATE: switcher save failed\n");

            } else {

                flipgb_exit_code =
                    FLIPGB_EXIT_SWITCHER;

                break;
            }
        }

        flip_hotkeys_result_t hotkey_result;

        if (flip_hotkeys_apply_actions(
                &control_state.system,
                &hotkey_result) < 0) {

            fprintf(
                stderr,
                "FLIPGB: global hotkey handling failed\n");
        }

        double measured_fps = 0.0;

        int fps_result =
            flip_timing_record_frame(
                &timing,
                &measured_fps);

        if (fps_result < 0) {
            fprintf(
                stderr,
                "FLIPGB: timing FPS measurement failed\n");
            break;
        }

        if (fps_result > 0) {
            fps_overlay_value =
                measured_fps;

            flip_video_set_fps_overlay(
                fps_overlay_enabled,
                fps_overlay_value);

            fprintf(
                stderr,
                "FLIPGB_FPS %.3f target=%.3f\n",
                measured_fps,
                fps);
        }

        /*
         * START+SELECT is interpreted by the shared controls
         * layer as a global FlipOS exit request.
         */
        if (control_state.system.exit_requested) {

            fprintf(
                stderr,
                "FLIPGB: START+SELECT exit\n");

            if (save_resume_state() < 0) {
                fprintf(
                    stderr,
                    "FLIPGB_STATE: exit save failed\n");
            }

            break;
        }

        int wait_result =
            flip_timing_wait_frame(
                &timing);

        if (wait_result < 0) {
            fprintf(
                stderr,
                "FLIPGB: timing wait failed\n");
            break;
        }

        if (wait_result > 0) {
            fprintf(
                stderr,
                "FLIPGB: frame schedule resync\n");
        }
    }

    unload_rom(&game);

    retro_deinit();

    flip_audio_close();

    flip_input_close();
    flip_video_close();

    fprintf(
        stderr,
        "FLIPGB: clean exit\n");

    return flipgb_exit_code;
}
