#define _GNU_SOURCE

#include "flip_gambatte_core.h"

#include "flip_audio.h"
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
    int initialized;
    int loaded;
} flip_gambatte_state_t;

static flip_gambatte_state_t gambatte;

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

                gambatte.pixel_format = fmt;

                fprintf(
                    stderr,
                    "FLIP_GAMBATTE: pixel_format=%d\n",
                    (int)fmt);

                return true;
            }

            fprintf(
                stderr,
                "FLIP_GAMBATTE: unsupported pixel format %d\n",
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
            *dir = "/mnt/SDCARD/flipos-dev/system/gb";
            return true;
        }

        case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
        {
            const char **dir = data;
            *dir = "/mnt/SDCARD/flipos-dev/saves/gb";
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

    if (gambatte.pixel_format ==
        RETRO_PIXEL_FORMAT_RGB565) {

        format = FLIP_VIDEO_FORMAT_RGB565;

    } else if (
        gambatte.pixel_format ==
        RETRO_PIXEL_FORMAT_XRGB8888) {

        format = FLIP_VIDEO_FORMAT_XRGB8888;

    } else {
        fprintf(
            stderr,
            "FLIP_GAMBATTE: unsupported active video format=%d\n",
            (int)gambatte.pixel_format);
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
            "FLIP_GAMBATTE: video submit failed\n");
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
            return gambatte.input.down[FLIP_GAME_UP];

        case RETRO_DEVICE_ID_JOYPAD_DOWN:
            return gambatte.input.down[FLIP_GAME_DOWN];

        case RETRO_DEVICE_ID_JOYPAD_LEFT:
            return gambatte.input.down[FLIP_GAME_LEFT];

        case RETRO_DEVICE_ID_JOYPAD_RIGHT:
            return gambatte.input.down[FLIP_GAME_RIGHT];

        case RETRO_DEVICE_ID_JOYPAD_A:
            return gambatte.input.down[FLIP_GAME_A];

        case RETRO_DEVICE_ID_JOYPAD_B:
            return gambatte.input.down[FLIP_GAME_B];

        case RETRO_DEVICE_ID_JOYPAD_START:
            return gambatte.input.down[FLIP_GAME_START];

        case RETRO_DEVICE_ID_JOYPAD_SELECT:
            return gambatte.input.down[FLIP_GAME_SELECT];

        default:
            return 0;
    }
}

static int gambatte_init(
    void *userdata)
{
    flip_gambatte_state_t *state = userdata;

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

static void gambatte_deinit(
    void *userdata)
{
    flip_gambatte_state_t *state = userdata;

    if (!state->initialized)
        return;

    retro_deinit();
    state->initialized = 0;
}

static int gambatte_load_game(
    void *userdata,
    const char *path)
{
    flip_gambatte_state_t *state = userdata;

    FILE *f = fopen(path, "rb");

    if (!f) {
        perror("FLIP_GAMBATTE: fopen ROM");
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

static void gambatte_unload_game(
    void *userdata)
{
    flip_gambatte_state_t *state = userdata;

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

static int gambatte_get_info(
    void *userdata,
    flip_core_info_t *info)
{
    flip_gambatte_state_t *state = userdata;

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
        (unsigned)(
            av.timing.sample_rate + 0.5);

    info->base_width =
        av.geometry.base_width;

    info->base_height =
        av.geometry.base_height;

    return 0;
}

static int gambatte_set_input(
    void *userdata,
    const flip_game_input_t *input)
{
    flip_gambatte_state_t *state = userdata;

    if (!input)
        return -1;

    state->input = *input;
    return 0;
}

static int gambatte_run_frame(
    void *userdata)
{
    flip_gambatte_state_t *state = userdata;

    if (!state->loaded)
        return -1;

    retro_run();
    return 0;
}

static size_t gambatte_serialize_size(
    void *userdata)
{
    flip_gambatte_state_t *state = userdata;

    if (!state->loaded)
        return 0;

    return retro_serialize_size();
}

static int gambatte_serialize(
    void *userdata,
    void *data,
    size_t size)
{
    flip_gambatte_state_t *state = userdata;

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

static int gambatte_unserialize(
    void *userdata,
    const void *data,
    size_t size)
{
    flip_gambatte_state_t *state = userdata;

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

static void *gambatte_save_memory_data(
    void *userdata)
{
    flip_gambatte_state_t *state = userdata;

    if (!state->loaded)
        return NULL;

    return retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
}

static size_t gambatte_save_memory_size(
    void *userdata)
{
    flip_gambatte_state_t *state = userdata;

    if (!state->loaded)
        return 0;

    return retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
}

static flip_core_adapter_t adapter = {
    .userdata = &gambatte,
    .init = gambatte_init,
    .deinit = gambatte_deinit,
    .load_game = gambatte_load_game,
    .unload_game = gambatte_unload_game,
    .get_info = gambatte_get_info,
    .set_input = gambatte_set_input,
    .run_frame = gambatte_run_frame,
    .serialize_size = gambatte_serialize_size,
    .serialize = gambatte_serialize,
    .unserialize = gambatte_unserialize,
    .save_memory_data = gambatte_save_memory_data,
    .save_memory_size = gambatte_save_memory_size,
};

flip_core_adapter_t *flip_gambatte_core_adapter(void)
{
    return &adapter;
}
