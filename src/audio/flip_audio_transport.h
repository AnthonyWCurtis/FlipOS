#ifndef FLIP_AUDIO_TRANSPORT_H
#define FLIP_AUDIO_TRANSPORT_H

#include <stddef.h>
#include <sys/types.h>

/*
 * Internal audio transport helpers.
 *
 * The PCM pipe must remain non-blocking so a wedged aplay/ALSA path cannot
 * indefinitely hold the emulator's shutdown sequence.
 */
int flip_audio_transport_set_nonblocking(int fd);

int flip_audio_transport_write_all(
    int fd,
    const void *data,
    size_t bytes,
    unsigned timeout_ms);

/*
 * Reap the aplay child without an unbounded wait. The child is first given a
 * clean-exit grace period, then SIGTERM, then SIGKILL. Returns 0 only after the
 * child has actually been reaped.
 */
int flip_audio_transport_shutdown_child(
    pid_t pid,
    unsigned grace_ms,
    unsigned term_ms,
    unsigned kill_ms,
    int *status);

#endif
