#define _GNU_SOURCE

#include "flip_libretro_core.h"

#include "flip_audio.h"
#include "flip_bios.h"
#include "flip_paths.h"
#include "flip_video.h"

#include "libretro/libretro.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    enum retro_pixel_format pixel_format;
    flip_game_input_t input;
    struct retro_game_info game;
    char system_directory[FLIP_BIOS_PATH_MAX];
    char save_directory[FLIP_BIOS_PATH_MAX];
    unsigned runtime_audio_sample_rate;
    int initialized;
    int loaded;
} flip_libretro_state_t;

static flip_libretro_state_t libretro_state;
static const flip_libretro_core_config_t *active_config;

static bool env_cb(
    unsigned cmd,
    void *data)
{
    switch (cmd) {
        case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
        {
            enum retro_pixel_format fmt =
                *(const enum retro_pixel_format *)data;

            if (fmt == RETRO_PIXEL_FORMAT_RGB565 ||
                fmt == RETRO_PIXEL_FORMAT_XRGB8888) {

                libretro_state.pixel_format = fmt;

                fprintf(
                    stderr,
                    "FLIP_LIBRETRO: pixel_format=%d\n",
                    (int)fmt);

                return true;
            }

            fprintf(
                stderr,
                "FLIP_LIBRETRO: unsupported pixel format %d\n",
                (int)fmt);

            return false;
        }

        case RETRO_ENVIRONMENT_GET_CAN_DUPE:
        {
            bool *can_dupe = data;
            *can_dupe = true;
            return true;
        }

        case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
        {
            const char **dir = data;

            if (!active_config ||
                !active_config->bios_system_id ||
                flip_bios_system_directory(
                    active_config->bios_system_id,
                    libretro_state.system_directory,
                    sizeof(libretro_state.system_directory)) < 0) {
                return false;
            }

            *dir = libretro_state.system_directory;
            return true;
        }

        case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
        {
            const char **dir = data;

            if (!active_config ||
                !active_config->storage_id) {
                return false;
            }

            int written = snprintf(
                libretro_state.save_directory,
                sizeof(libretro_state.save_directory),
                "%s/%s",
                flip_paths_save_root(),
                active_config->storage_id);

            if (written < 0 ||
                (size_t)written >= sizeof(libretro_state.save_directory)) {
                return false;
            }

            *dir = libretro_state.save_directory;
            return true;
        }

        case RETRO_ENVIRONMENT_GET_VARIABLE:
        {
            struct retro_variable *var = data;
            var->value = NULL;
            return true;
        }

        case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
        {
            bool *updated = data;
            *updated = false;
            return true;
        }

        case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
        {
            const struct retro_system_av_info *av = data;

            if (!av ||
                av->timing.sample_rate <= 0.0) {

                return false;
            }

            unsigned sample_rate =
                (unsigned)(
                    av->timing.sample_rate +
                    0.5);

            if (sample_rate == 0)
                return false;

            if (sample_rate !=
                libretro_state.runtime_audio_sample_rate) {

                unsigned previous_rate =
                    libretro_state.runtime_audio_sample_rate;

                libretro_state.runtime_audio_sample_rate =
                    sample_rate;

                fprintf(
                    stderr,
                    "FLIP_LIBRETRO: runtime audio rate %u->%u\n",
                    previous_rate,
                    sample_rate);

                if (flip_audio_set_source_rate(
                        sample_rate) < 0) {

                    fprintf(
                        stderr,
                        "FLIP_LIBRETRO: audio rate update failed rate=%u\n",
                        sample_rate);
                }
            }

            return true;
        }

        case RETRO_ENVIRONMENT_SET_VARIABLES:
        case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
        case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
        case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
        case RETRO_ENVIRONMENT_SET_MEMORY_MAPS:
        case RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS:
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

    if (libretro_state.pixel_format ==
        RETRO_PIXEL_FORMAT_RGB565) {

        format = FLIP_VIDEO_FORMAT_RGB565;

    } else if (
        libretro_state.pixel_format ==
        RETRO_PIXEL_FORMAT_XRGB8888) {

        format = FLIP_VIDEO_FORMAT_XRGB8888;

    } else {
        fprintf(
            stderr,
            "FLIP_LIBRETRO: unsupported active video format=%d\n",
            (int)libretro_state.pixel_format);
        return;
    }

    if (flip_video_submit(
            data,
            width,
            height,
            pitch,
            format) < 0) {

        fprintf(
            stderr,
            "FLIP_LIBRETRO: video submit failed\n");
    }
}

static void audio_sample_cb(
    int16_t left,
    int16_t right)
{
    int16_t frame[2] = {
        left,
        right,
    };

    flip_audio_submit(
        frame,
        1);
}

static size_t audio_batch_cb(
    const int16_t *data,
    size_t frames)
{
    return flip_audio_submit(
        data,
        frames);
}

static void input_poll_cb(void)
{
    /* FlipOS Core Host captures one coherent input snapshot. */
}

static int16_t input_state_cb(
    unsigned port,
    unsigned device,
    unsigned index,
    unsigned id)
{
    (void)index;

    if (port != 0 ||
        device != RETRO_DEVICE_JOYPAD)
        return 0;

    switch (id) {
        case RETRO_DEVICE_ID_JOYPAD_UP:
            return libretro_state.input.down[FLIP_GAME_UP];

        case RETRO_DEVICE_ID_JOYPAD_DOWN:
            return libretro_state.input.down[FLIP_GAME_DOWN];

        case RETRO_DEVICE_ID_JOYPAD_LEFT:
            return libretro_state.input.down[FLIP_GAME_LEFT];

        case RETRO_DEVICE_ID_JOYPAD_RIGHT:
            return libretro_state.input.down[FLIP_GAME_RIGHT];

        case RETRO_DEVICE_ID_JOYPAD_A:
            return libretro_state.input.down[FLIP_GAME_A];

        case RETRO_DEVICE_ID_JOYPAD_B:
            return libretro_state.input.down[FLIP_GAME_B];

        case RETRO_DEVICE_ID_JOYPAD_START:
            return libretro_state.input.down[FLIP_GAME_START];

        case RETRO_DEVICE_ID_JOYPAD_SELECT:
            return libretro_state.input.down[FLIP_GAME_SELECT];

        case RETRO_DEVICE_ID_JOYPAD_L:
            return libretro_state.input.down[FLIP_GAME_L1];

        case RETRO_DEVICE_ID_JOYPAD_R:
            return libretro_state.input.down[FLIP_GAME_R1];

        default:
            return 0;
    }
}

static int libretro_init(
    void *userdata)
{
    flip_libretro_state_t *state = userdata;

    memset(
        state,
        0,
        sizeof(*state));

    state->pixel_format =
        RETRO_PIXEL_FORMAT_RGB565;

    retro_set_environment(env_cb);
    retro_set_video_refresh(video_cb);
    retro_set_audio_sample(audio_sample_cb);
    retro_set_audio_sample_batch(audio_batch_cb);
    retro_set_input_poll(input_poll_cb);
    retro_set_input_state(input_state_cb);

    retro_init();
    state->initialized = 1;

    return 0;
}

static void libretro_deinit(
    void *userdata)
{
    flip_libretro_state_t *state = userdata;

    if (!state->initialized)
        return;

    retro_deinit();
    state->initialized = 0;
}

static int libretro_load_game(
    void *userdata,
    const char *path)
{
    flip_libretro_state_t *state = userdata;

    FILE *f = fopen(path, "rb");

    if (!f) {
        perror("FLIP_LIBRETRO: fopen ROM");
        return -1;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -1;
    }

    long len = ftell(f);

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
        &state->game,
        0,
        sizeof(state->game));

    state->game.path = path;
    state->game.data = data;
    state->game.size = (size_t)len;
    state->game.meta = NULL;

    if (!retro_load_game(
            &state->game)) {

        free(data);
        memset(
            &state->game,
            0,
            sizeof(state->game));
        return -1;
    }

    state->loaded = 1;
    return 0;
}

static void libretro_unload_game(
    void *userdata)
{
    flip_libretro_state_t *state = userdata;

    if (state->loaded) {
        retro_unload_game();
        state->loaded = 0;
    }

    if (state->game.data) {
        free((void *)state->game.data);
        state->game.data = NULL;
    }

    memset(
        &state->game,
        0,
        sizeof(state->game));
}

static int libretro_get_info(
    void *userdata,
    flip_core_info_t *info)
{
    flip_libretro_state_t *state = userdata;

    if (!state->initialized ||
        !state->loaded ||
        !info)
        return -1;

    struct retro_system_info system_info;
    struct retro_system_av_info av;

    memset(
        &system_info,
        0,
        sizeof(system_info));

    memset(
        &av,
        0,
        sizeof(av));

    retro_get_system_info(
        &system_info);

    retro_get_system_av_info(
        &av);

    memset(
        info,
        0,
        sizeof(*info));

    info->core_name =
        system_info.library_name;

    info->core_version =
        system_info.library_version;

    info->target_fps =
        av.timing.fps;

    info->audio_sample_rate =
        state->runtime_audio_sample_rate
        ? state->runtime_audio_sample_rate
        : (unsigned)(
            av.timing.sample_rate + 0.5);

    info->base_width =
        av.geometry.base_width;

    info->base_height =
        av.geometry.base_height;

    return 0;
}

static int libretro_set_input(
    void *userdata,
    const flip_game_input_t *input)
{
    flip_libretro_state_t *state = userdata;

    if (!input)
        return -1;

    state->input = *input;
    return 0;
}

static int libretro_run_frame(
    void *userdata)
{
    flip_libretro_state_t *state = userdata;

    if (!state->loaded)
        return -1;

    retro_run();
    return 0;
}

static size_t libretro_serialize_size(
    void *userdata)
{
    flip_libretro_state_t *state = userdata;

    if (!state->loaded)
        return 0;

    return retro_serialize_size();
}

static int libretro_serialize(
    void *userdata,
    void *data,
    size_t size)
{
    flip_libretro_state_t *state = userdata;

    if (!state->loaded ||
        !data ||
        size == 0)
        return -1;

    return retro_serialize(
        data,
        size)
        ? 0
        : -1;
}

static int libretro_unserialize(
    void *userdata,
    const void *data,
    size_t size)
{
    flip_libretro_state_t *state = userdata;

    if (!state->loaded ||
        !data ||
        size == 0)
        return -1;

    return retro_unserialize(
        data,
        size)
        ? 0
        : -1;
}

static void *libretro_save_memory_data(
    void *userdata)
{
    flip_libretro_state_t *state = userdata;

    if (!state->loaded)
        return NULL;

    return retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
}

static size_t libretro_save_memory_size(
    void *userdata)
{
    flip_libretro_state_t *state = userdata;

    if (!state->loaded)
        return 0;

    return retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
}

static flip_core_adapter_t adapter = {
    .userdata = &libretro_state,
    .init = libretro_init,
    .deinit = libretro_deinit,
    .load_game = libretro_load_game,
    .unload_game = libretro_unload_game,
    .get_info = libretro_get_info,
    .set_input = libretro_set_input,
    .run_frame = libretro_run_frame,
    .serialize_size = libretro_serialize_size,
    .serialize = libretro_serialize,
    .unserialize = libretro_unserialize,
    .save_memory_data = libretro_save_memory_data,
    .save_memory_size = libretro_save_memory_size,
};

flip_core_adapter_t *flip_libretro_core_adapter(
    const flip_libretro_core_config_t *config)
{
    if (!config ||
        !config->bios_system_id ||
        !config->storage_id)
        return NULL;

    active_config = config;
    return &adapter;
}
