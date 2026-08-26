#ifndef FLIP_CORE_H
#define FLIP_CORE_H

#include "flip_controls.h"

#include <stddef.h>

typedef struct {
    const char *core_name;
    const char *core_version;

    double target_fps;
    unsigned audio_sample_rate;

    unsigned base_width;
    unsigned base_height;
} flip_core_info_t;

typedef struct {
    void *userdata;

    int (*init)(void *userdata);
    void (*deinit)(void *userdata);

    int (*load_game)(
        void *userdata,
        const char *path);

    void (*unload_game)(
        void *userdata);

    int (*get_info)(
        void *userdata,
        flip_core_info_t *info);

    int (*set_input)(
        void *userdata,
        const flip_game_input_t *input);

    int (*run_frame)(
        void *userdata);

    size_t (*serialize_size)(
        void *userdata);

    int (*serialize)(
        void *userdata,
        void *data,
        size_t size);

    int (*unserialize)(
        void *userdata,
        const void *data,
        size_t size);
} flip_core_adapter_t;

int flip_core_validate(
    const flip_core_adapter_t *adapter);

int flip_core_init(
    flip_core_adapter_t *adapter);

void flip_core_deinit(
    flip_core_adapter_t *adapter);

int flip_core_load_game(
    flip_core_adapter_t *adapter,
    const char *path);

void flip_core_unload_game(
    flip_core_adapter_t *adapter);

int flip_core_get_info(
    flip_core_adapter_t *adapter,
    flip_core_info_t *info);

int flip_core_set_input(
    flip_core_adapter_t *adapter,
    const flip_game_input_t *input);

int flip_core_run_frame(
    flip_core_adapter_t *adapter);

size_t flip_core_serialize_size(
    flip_core_adapter_t *adapter);

int flip_core_serialize(
    flip_core_adapter_t *adapter,
    void *data,
    size_t size);

int flip_core_unserialize(
    flip_core_adapter_t *adapter,
    const void *data,
    size_t size);

#endif
