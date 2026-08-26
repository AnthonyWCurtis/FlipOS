#ifndef FLIP_AUDIO_H
#define FLIP_AUDIO_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    unsigned sample_rate;
    unsigned channels;
} flip_audio_config_t;

typedef struct {
    uint64_t submit_total_us;
    uint64_t submit_max_us;
    uint64_t submit_count;

    uint64_t transport_write_total_us;
    uint64_t transport_write_max_us;
    uint64_t transport_write_count;
} flip_audio_diag_snapshot_t;

int flip_audio_open(
    const flip_audio_config_t *config);

size_t flip_audio_submit(
    const int16_t *samples,
    size_t frames);

int flip_audio_end_frame(void);

void flip_audio_diag_snapshot(
    flip_audio_diag_snapshot_t *snapshot);

void flip_audio_diag_reset(void);

void flip_audio_close(void);

int flip_audio_set_volume(int percent);
int flip_audio_get_volume(void);

#endif
