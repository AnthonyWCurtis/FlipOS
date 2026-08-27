#include "flip_session_adapter.h"

#include <string.h>

static int test_resume_available(
    const char *rom_path)
{
    return rom_path &&
        strstr(rom_path, "has-resume") != NULL;
}

static const flip_session_adapter_t test_adapter = {
    .system = "GB",
    .resume_available = test_resume_available
};

const flip_session_adapter_t *
flip_session_adapter_for_system(
    const char *system)
{
    if (system &&
        strcmp(system, "GB") == 0)
        return &test_adapter;

    return NULL;
}
