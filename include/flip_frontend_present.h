#ifndef FLIP_FRONTEND_PRESENT_H
#define FLIP_FRONTEND_PRESENT_H

#include "flip_drm.h"
#include "flip_frontend_idle.h"

void flip_frontend_present_set_idle(
    flip_frontend_idle_t *idle);

int flip_frontend_present(
    flip_drm_t *drm);

#endif
