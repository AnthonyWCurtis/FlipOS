#ifndef FLIP_OPTIMIZER_QUIESCE_H
#define FLIP_OPTIMIZER_QUIESCE_H

#include <stddef.h>
#include <sys/types.h>

#define FLIP_OPTIMIZER_QUIESCE_MAX_PIDS 32

typedef struct {
    pid_t pids[FLIP_OPTIMIZER_QUIESCE_MAX_PIDS];
    size_t count;
    int active;
} flip_optimizer_quiesce_t;

int flip_optimizer_quiesce_cmdline_is_writer(
    const char *cmdline);

int flip_optimizer_quiesce_begin(
    flip_optimizer_quiesce_t *state);

int flip_optimizer_quiesce_end(
    flip_optimizer_quiesce_t *state);

#endif
