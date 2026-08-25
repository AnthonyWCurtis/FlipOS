#ifndef FLIP_AUDIO_H
#define FLIP_AUDIO_H

#include <stddef.h>
#include <stdint.h>

int flip_audio_open(void);

size_t flip_audio_write(
    const int16_t *samples,
    size_t frames);

void flip_audio_close(void);

int flip_audio_set_volume(int percent);
int flip_audio_get_volume(void);

#endif
