#include "flip_core.h"

int flip_core_validate(
    const flip_core_adapter_t *adapter)
{
    if (!adapter ||
        !adapter->init ||
        !adapter->deinit ||
        !adapter->load_game ||
        !adapter->unload_game ||
        !adapter->get_info ||
        !adapter->set_input ||
        !adapter->run_frame ||
        !adapter->serialize_size ||
        !adapter->serialize ||
        !adapter->unserialize) {

        return -1;
    }

    return 0;
}

int flip_core_init(
    flip_core_adapter_t *adapter)
{
    if (flip_core_validate(adapter) < 0)
        return -1;

    return adapter->init(
        adapter->userdata);
}

void flip_core_deinit(
    flip_core_adapter_t *adapter)
{
    if (!adapter ||
        !adapter->deinit)
        return;

    adapter->deinit(
        adapter->userdata);
}

int flip_core_load_game(
    flip_core_adapter_t *adapter,
    const char *path)
{
    if (flip_core_validate(adapter) < 0 ||
        !path ||
        !path[0]) {

        return -1;
    }

    return adapter->load_game(
        adapter->userdata,
        path);
}

void flip_core_unload_game(
    flip_core_adapter_t *adapter)
{
    if (!adapter ||
        !adapter->unload_game)
        return;

    adapter->unload_game(
        adapter->userdata);
}

int flip_core_get_info(
    flip_core_adapter_t *adapter,
    flip_core_info_t *info)
{
    if (flip_core_validate(adapter) < 0 ||
        !info) {

        return -1;
    }

    return adapter->get_info(
        adapter->userdata,
        info);
}

int flip_core_set_input(
    flip_core_adapter_t *adapter,
    const flip_game_input_t *input)
{
    if (flip_core_validate(adapter) < 0 ||
        !input) {

        return -1;
    }

    return adapter->set_input(
        adapter->userdata,
        input);
}

int flip_core_run_frame(
    flip_core_adapter_t *adapter)
{
    if (flip_core_validate(adapter) < 0)
        return -1;

    return adapter->run_frame(
        adapter->userdata);
}

size_t flip_core_serialize_size(
    flip_core_adapter_t *adapter)
{
    if (flip_core_validate(adapter) < 0)
        return 0;

    return adapter->serialize_size(
        adapter->userdata);
}

int flip_core_serialize(
    flip_core_adapter_t *adapter,
    void *data,
    size_t size)
{
    if (flip_core_validate(adapter) < 0 ||
        !data ||
        size == 0) {

        return -1;
    }

    return adapter->serialize(
        adapter->userdata,
        data,
        size);
}

int flip_core_unserialize(
    flip_core_adapter_t *adapter,
    const void *data,
    size_t size)
{
    if (flip_core_validate(adapter) < 0 ||
        !data ||
        size == 0) {

        return -1;
    }

    return adapter->unserialize(
        adapter->userdata,
        data,
        size);
}

void *flip_core_save_memory_data(
    flip_core_adapter_t *adapter)
{
    if (!adapter ||
        !adapter->save_memory_data)
        return NULL;

    return adapter->save_memory_data(
        adapter->userdata);
}

size_t flip_core_save_memory_size(
    flip_core_adapter_t *adapter)
{
    if (!adapter ||
        !adapter->save_memory_size)
        return 0;

    return adapter->save_memory_size(
        adapter->userdata);
}

void *flip_core_rtc_memory_data(
    flip_core_adapter_t *adapter)
{
    if (!adapter ||
        !adapter->rtc_memory_data)
        return NULL;

    return adapter->rtc_memory_data(
        adapter->userdata);
}

size_t flip_core_rtc_memory_size(
    flip_core_adapter_t *adapter)
{
    if (!adapter ||
        !adapter->rtc_memory_size)
        return 0;

    return adapter->rtc_memory_size(
        adapter->userdata);
}
