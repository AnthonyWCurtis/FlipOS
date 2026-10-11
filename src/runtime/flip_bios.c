#include "flip_bios.h"
#include "flip_paths.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

typedef struct {
    flip_bios_kind_t kind;
    const char *system_id;
    const char *filename;
    size_t expected_size;
    const char *expected_sha256;
} flip_bios_definition_t;

/*
 * Known-good SHA-256 digests for the standard retail BIOS images.
 * FlipOS never ships or downloads these files; it only verifies
 * user-supplied copies against these fingerprints.
 */
static const flip_bios_definition_t BIOS_DEFINITIONS[] = {
    {
        .kind = FLIP_BIOS_GB_DMG,
        .system_id = "GB",
        .filename = "gb_bios.bin",
        .expected_size = 256,
        .expected_sha256 =
            "cf053eccb4ccafff9e67339d4e78e98dce7d1ed59be819d2a1ba2232c6fce1c7",
    },
    {
        .kind = FLIP_BIOS_GB_CGB,
        .system_id = "GB",
        .filename = "gbc_bios.bin",
        .expected_size = 2304,
        .expected_sha256 =
            "b4f2e416a35eef52cba161b159c7c8523a92594facb924b3ede0d722867c50c7",
    },
    {
        .kind = FLIP_BIOS_GBA,
        .system_id = "GBA",
        .filename = "gba_bios.bin",
        .expected_size = 16384,
        .expected_sha256 =
            "fd2547724b505f487e6dcb29ec2ecff3af35a841a77ab2e85fd87350abd36570",
    },
};

typedef struct {
    uint32_t state[8];
    uint64_t bit_count;
    unsigned char block[64];
    size_t block_used;
} sha256_ctx_t;

static uint32_t rotr32(uint32_t value, unsigned int bits)
{
    return (value >> bits) | (value << (32U - bits));
}

static void sha256_transform(sha256_ctx_t *ctx, const unsigned char block[64])
{
    static const uint32_t k[64] = {
        0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
        0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
        0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
        0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
        0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
        0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
        0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
        0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
        0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
        0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
        0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
        0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
        0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
        0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
        0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
        0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
    };

    uint32_t w[64];

    for (size_t i = 0; i < 16; ++i) {
        size_t j = i * 4;
        w[i] =
            ((uint32_t)block[j] << 24) |
            ((uint32_t)block[j + 1] << 16) |
            ((uint32_t)block[j + 2] << 8) |
            ((uint32_t)block[j + 3]);
    }

    for (size_t i = 16; i < 64; ++i) {
        uint32_t s0 =
            rotr32(w[i - 15], 7) ^
            rotr32(w[i - 15], 18) ^
            (w[i - 15] >> 3);

        uint32_t s1 =
            rotr32(w[i - 2], 17) ^
            rotr32(w[i - 2], 19) ^
            (w[i - 2] >> 10);

        w[i] =
            w[i - 16] +
            s0 +
            w[i - 7] +
            s1;
    }

    uint32_t a = ctx->state[0];
    uint32_t b = ctx->state[1];
    uint32_t c = ctx->state[2];
    uint32_t d = ctx->state[3];
    uint32_t e = ctx->state[4];
    uint32_t f = ctx->state[5];
    uint32_t g = ctx->state[6];
    uint32_t h = ctx->state[7];

    for (size_t i = 0; i < 64; ++i) {
        uint32_t s1 =
            rotr32(e, 6) ^
            rotr32(e, 11) ^
            rotr32(e, 25);

        uint32_t ch =
            (e & f) ^
            ((~e) & g);

        uint32_t temp1 =
            h + s1 + ch + k[i] + w[i];

        uint32_t s0 =
            rotr32(a, 2) ^
            rotr32(a, 13) ^
            rotr32(a, 22);

        uint32_t maj =
            (a & b) ^
            (a & c) ^
            (b & c);

        uint32_t temp2 =
            s0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

static void sha256_init(sha256_ctx_t *ctx)
{
    static const uint32_t initial[8] = {
        0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
        0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U,
    };

    memcpy(ctx->state, initial, sizeof(initial));
    ctx->bit_count = 0;
    ctx->block_used = 0;
}

static void sha256_update(
    sha256_ctx_t *ctx,
    const unsigned char *data,
    size_t length)
{
    ctx->bit_count += (uint64_t)length * 8U;

    while (length > 0) {
        size_t space =
            sizeof(ctx->block) - ctx->block_used;

        size_t take =
            length < space
                ? length
                : space;

        memcpy(
            ctx->block + ctx->block_used,
            data,
            take);

        ctx->block_used += take;
        data += take;
        length -= take;

        if (ctx->block_used == sizeof(ctx->block)) {
            sha256_transform(ctx, ctx->block);
            ctx->block_used = 0;
        }
    }
}

static void sha256_final(
    sha256_ctx_t *ctx,
    unsigned char digest[32])
{
    ctx->block[ctx->block_used++] = 0x80U;

    if (ctx->block_used > 56) {
        while (ctx->block_used < 64)
            ctx->block[ctx->block_used++] = 0;

        sha256_transform(ctx, ctx->block);
        ctx->block_used = 0;
    }

    while (ctx->block_used < 56)
        ctx->block[ctx->block_used++] = 0;

    for (int i = 7; i >= 0; --i) {
        ctx->block[ctx->block_used++] =
            (unsigned char)(
                (ctx->bit_count >> (i * 8)) &
                0xffU);
    }

    sha256_transform(ctx, ctx->block);

    for (size_t i = 0; i < 8; ++i) {
        digest[i * 4] =
            (unsigned char)(ctx->state[i] >> 24);
        digest[i * 4 + 1] =
            (unsigned char)(ctx->state[i] >> 16);
        digest[i * 4 + 2] =
            (unsigned char)(ctx->state[i] >> 8);
        digest[i * 4 + 3] =
            (unsigned char)ctx->state[i];
    }
}

static int sha256_file(
    const char *path,
    char out[FLIP_BIOS_SHA256_HEX])
{
    FILE *file = fopen(path, "rb");

    if (!file)
        return -1;

    sha256_ctx_t ctx;
    sha256_init(&ctx);

    unsigned char buffer[4096];

    for (;;) {
        size_t count =
            fread(
                buffer,
                1,
                sizeof(buffer),
                file);

        if (count > 0)
            sha256_update(
                &ctx,
                buffer,
                count);

        if (count < sizeof(buffer)) {
            if (ferror(file)) {
                fclose(file);
                return -1;
            }

            break;
        }
    }

    if (fclose(file) != 0)
        return -1;

    unsigned char digest[32];
    sha256_final(&ctx, digest);

    static const char hex[] =
        "0123456789abcdef";

    for (size_t i = 0; i < sizeof(digest); ++i) {
        out[i * 2] =
            hex[digest[i] >> 4];
        out[i * 2 + 1] =
            hex[digest[i] & 0x0fU];
    }

    out[64] = '\0';
    return 0;
}

static const flip_bios_definition_t *find_definition(
    flip_bios_kind_t kind)
{
    for (size_t i = 0;
         i < sizeof(BIOS_DEFINITIONS) / sizeof(BIOS_DEFINITIONS[0]);
         ++i) {
        if (BIOS_DEFINITIONS[i].kind == kind)
            return &BIOS_DEFINITIONS[i];
    }

    return NULL;
}

static const char *status_name(
    flip_bios_status_t status)
{
    switch (status) {
    case FLIP_BIOS_MISSING:
        return "MISSING";
    case FLIP_BIOS_VALID:
        return "VALID";
    case FLIP_BIOS_INVALID:
        return "INVALID";
    case FLIP_BIOS_ERROR:
        return "ERROR";
    }

    return "ERROR";
}

static const char *kind_name(
    flip_bios_kind_t kind)
{
    switch (kind) {
    case FLIP_BIOS_GB_DMG:
        return "GB_DMG";
    case FLIP_BIOS_GB_CGB:
        return "GB_CGB";
    case FLIP_BIOS_GBA:
        return "GBA";
    }

    return "UNKNOWN";
}

int flip_bios_system_directory(
    const char *system_id,
    char *path,
    size_t path_size)
{
    if (!system_id || !path || path_size == 0)
        return -1;

    const char *directory_name = NULL;

    if (strcmp(system_id, "GB") == 0)
        directory_name = "gb";
    else if (strcmp(system_id, "GBA") == 0)
        directory_name = "gba";
    else
        return -1;

    int written = snprintf(
        path,
        path_size,
        "%s/%s",
        flip_paths_system_root(),
        directory_name);

    if (written < 0 ||
        (size_t)written >= path_size)
        return -1;

    return 0;
}

int flip_bios_query(
    flip_bios_kind_t kind,
    flip_bios_info_t *info)
{
    if (!info)
        return -1;

    const flip_bios_definition_t *definition =
        find_definition(kind);

    if (!definition)
        return -1;

    memset(info, 0, sizeof(*info));

    info->kind = definition->kind;
    info->status = FLIP_BIOS_ERROR;
    info->system_id = definition->system_id;
    info->filename = definition->filename;
    info->expected_size = definition->expected_size;
    info->expected_sha256 = definition->expected_sha256;

    char directory[FLIP_BIOS_PATH_MAX];

    if (flip_bios_system_directory(
            definition->system_id,
            directory,
            sizeof(directory)) < 0)
        goto done;

    int written = snprintf(
        info->path,
        sizeof(info->path),
        "%s/%s",
        directory,
        definition->filename);

    if (written < 0 ||
        (size_t)written >= sizeof(info->path))
        goto done;

    struct stat st;

    if (stat(info->path, &st) < 0) {
        info->status =
            errno == ENOENT
                ? FLIP_BIOS_MISSING
                : FLIP_BIOS_ERROR;
        goto done;
    }

    if (!S_ISREG(st.st_mode) ||
        st.st_size < 0) {

        info->status =
            FLIP_BIOS_INVALID;

        goto done;
    }

    info->actual_size =
        (size_t)st.st_size;

    if (info->actual_size !=
        info->expected_size) {

        info->status =
            FLIP_BIOS_INVALID;

        goto done;
    }

    if (sha256_file(
            info->path,
            info->actual_sha256) < 0) {

        info->status =
            FLIP_BIOS_ERROR;

        goto done;
    }

    info->status =
        strcmp(
            info->actual_sha256,
            info->expected_sha256) == 0
            ? FLIP_BIOS_VALID
            : FLIP_BIOS_INVALID;

done:
    fprintf(
        stderr,
        "BIOS: kind=%s status=%s expected=%zu actual=%zu sha256=%s path=%s\n",
        kind_name(info->kind),
        status_name(info->status),
        info->expected_size,
        info->actual_size,
        info->actual_sha256[0]
            ? info->actual_sha256
            : "(none)",
        info->path[0]
            ? info->path
            : "(unavailable)");

    return 0;
}
