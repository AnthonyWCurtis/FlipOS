#define _GNU_SOURCE

#include "flip_core.h"
#include "flip_core_host.h"
#include "flip_gambatte_core.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define GB_STATE_DIR \
    "/mnt/SDCARD/flipos-dev/states/gb"

typedef struct {
    const char *rom_path;
} gb_state_context_t;

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
    const gb_state_context_t *context,
    char *path,
    size_t path_size)
{
    if (!context ||
        !context->rom_path ||
        !path ||
        path_size == 0)
        return -1;

    uint64_t hash =
        state_path_hash(
            context->rom_path);

    snprintf(
        path,
        path_size,
        GB_STATE_DIR "/%016llx.state",
        (unsigned long long)hash);

    return 0;
}

static int save_resume_state(
    flip_core_adapter_t *core,
    void *userdata)
{
    gb_state_context_t *context =
        userdata;

    size_t size =
        flip_core_serialize_size(
            core);

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

    if (flip_core_serialize(
            core,
            data,
            size) < 0) {

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
            context,
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

static int load_resume_state(
    flip_core_adapter_t *core,
    void *userdata)
{
    gb_state_context_t *context =
        userdata;

    char path[512];

    if (build_state_path(
            context,
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
        flip_core_serialize_size(
            core);

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
        flip_core_unserialize(
            core,
            data,
            expected);

    free(data);

    if (ok < 0) {
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

int main(
    int argc,
    char **argv)
{
    if (argc < 2) {
        fprintf(
            stderr,
            "Usage: %s game.gb\n",
            argv[0]);
        return 1;
    }

    gb_state_context_t state_context = {
        .rom_path = argv[1],
    };

    fprintf(
        stderr,
        "FLIPGB: rom=%s\n",
        state_context.rom_path);

    flip_core_adapter_t *core =
        flip_gambatte_core_adapter();

    flip_core_host_config_t config = {
        .core = core,
        .rom_path = state_context.rom_path,
        .fallback_fps = 59.7275,
        .save_state = save_resume_state,
        .restore_state = load_resume_state,
        .state_userdata = &state_context,
    };

    int exit_code = 0;

    if (flip_core_host_run(
            &config,
            &exit_code) < 0) {

        fprintf(
            stderr,
            "FLIPGB: core host failed\n");
        return 1;
    }

    fprintf(
        stderr,
        "FLIPGB: clean exit\n");

    return exit_code;
}
