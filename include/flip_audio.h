#ifndef FLIP_AUDIO_H
#define FLIP_AUDIO_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    unsigned sample_rate;
    unsigned channels;
} flip_audio_config_t;

int flip_audio_open(
    const flip_audio_config_t *config);

size_t flip_audio_submit(
    const int16_t *samples,
    size_t frames);

int flip_audio_end_frame(void);

void flip_audio_close(void);

int flip_audio_set_volume(int percent);
int flip_audio_get_volume(void);

#endif
