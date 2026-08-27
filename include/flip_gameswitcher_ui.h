#ifndef FLIP_GAMESWITCHER_UI_H
#define FLIP_GAMESWITCHER_UI_H

#include "flip_drm.h"
#include "flip_session_manager.h"

typedef enum {
    FLIP_GAMESWITCHER_BACK = 0,
    FLIP_GAMESWITCHER_NEW_GAME,
    FLIP_GAMESWITCHER_RESUME,
    FLIP_GAMESWITCHER_CLOSE,
    FLIP_GAMESWITCHER_ERROR
} flip_gameswitcher_action_t;

typedef struct {
    flip_gameswitcher_action_t action;
    char system[32];
    char rom[512];
} flip_gameswitcher_result_t;

typedef struct {
    flip_retained_session_list_t sessions;
} flip_gameswitcher_model_t;

int flip_gameswitcher_model_load(
    flip_gameswitcher_model_t *model);

size_t flip_gameswitcher_model_count(
    const flip_gameswitcher_model_t *model);

const flip_retained_session_t *
flip_gameswitcher_model_session_at(
    const flip_gameswitcher_model_t *model,
    size_t index);

void flip_gameswitcher_model_free(
    flip_gameswitcher_model_t *model);

int flip_gameswitcher_model_can_resume(
    const flip_gameswitcher_model_t *model,
    size_t index);

int flip_gameswitcher_model_can_close(
    const flip_gameswitcher_model_t *model,
    size_t index);

int flip_gameswitcher_run(
    flip_drm_t *drm,
    flip_gameswitcher_result_t *result);

#endif
