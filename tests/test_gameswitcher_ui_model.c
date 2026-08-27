#include "flip_gameswitcher_ui.h"
#include "flip_frontend_global_controls.h"
#include "flip_session_manager.h"
#include "flip_input.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>


void flip_draw_clear(
    flip_drm_t *drm,
    uint32_t colour)
{
    (void)drm;
    (void)colour;
}

void flip_draw_rect(
    flip_drm_t *drm,
    int x,
    int y,
    int w,
    int h,
    uint32_t colour)
{
    (void)drm;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)colour;
}

void flip_draw_rect_outline(
    flip_drm_t *drm,
    int x,
    int y,
    int w,
    int h,
    int thickness,
    uint32_t colour)
{
    (void)drm;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)thickness;
    (void)colour;
}

void flip_draw_text(
    flip_drm_t *drm,
    int x,
    int y,
    const char *text,
    int scale,
    uint32_t colour)
{
    (void)drm;
    (void)x;
    (void)y;
    (void)text;
    (void)scale;
    (void)colour;
}

int flip_drm_present(
    flip_drm_t *drm)
{
    (void)drm;
    return 0;
}

int flip_input_poll(
    flip_input_state_t *state,
    int timeout_ms)
{
    (void)state;
    (void)timeout_ms;
    return -1;
}

static int fixture_mode = 0;
static int refresh_calls = 0;
static int remove_calls = 0;
static int record_calls = 0;

static flip_retained_session_t retained_fixture[] = {
    {
        .system = "GB",
        .rom = "/roms/newest.gb",
        .status = FLIP_RETAINED_SESSION_SUSPENDED,
        .last_active = 300,
        .resume_available = 1
    },
    {
        .system = "GBC",
        .rom = "/roms/middle.gbc",
        .status = FLIP_RETAINED_SESSION_SUSPENDED,
        .last_active = 200,
        .resume_available = 1
    },
    {
        .system = "GB",
        .rom = "/roms/oldest.gb",
        .status = FLIP_RETAINED_SESSION_SUSPENDED,
        .last_active = 100,
        .resume_available = 0
    }
};

int flip_session_manager_refresh(
    flip_retained_session_list_t *list)
{
    assert(list != NULL);

    refresh_calls++;

    if (fixture_mode == 1) {
        list->items = retained_fixture;
        list->count =
            sizeof(retained_fixture) /
            sizeof(retained_fixture[0]);
        return 0;
    }

    list->items = NULL;
    list->count = 0;

    return 0;
}

void flip_session_manager_list_free(
    flip_retained_session_list_t *list)
{
    assert(list != NULL);

    list->items = NULL;
    list->count = 0;
}


int flip_session_manager_remove(
    const char *system_id,
    const char *rom_path)
{
    (void)system_id;
    (void)rom_path;

    remove_calls++;
    return 0;
}

int flip_session_manager_record(
    const char *system_id,
    const char *rom_path,
    flip_retained_session_status_t status,
    long long last_active)
{
    (void)system_id;
    (void)rom_path;
    (void)status;
    (void)last_active;

    record_calls++;
    return 0;
}

static void test_new_game_does_not_mutate_session_registry(void)
{
    fixture_mode = 0;
    refresh_calls = 0;
    remove_calls = 0;
    record_calls = 0;

    flip_gameswitcher_result_t result;

    assert(
        flip_gameswitcher_run(
            NULL,
            &result) == 0);

    assert(
        result.action ==
        FLIP_GAMESWITCHER_NEW_GAME);

    assert(refresh_calls == 1);
    assert(remove_calls == 0);
    assert(record_calls == 0);
}

static void test_empty_sessions_offer_new_game(void)
{
    fixture_mode = 0;

    flip_gameswitcher_result_t result;

    assert(
        flip_gameswitcher_run(
            NULL,
            &result) == 0);

    assert(
        result.action ==
        FLIP_GAMESWITCHER_NEW_GAME);
}

static void test_model_preserves_most_recent_session_order(void)
{
    fixture_mode = 1;

    flip_gameswitcher_model_t model;

    assert(
        flip_gameswitcher_model_load(
            &model) == 0);

    assert(
        flip_gameswitcher_model_count(
            &model) == 3);

    const flip_retained_session_t *first =
        flip_gameswitcher_model_session_at(
            &model,
            0);

    const flip_retained_session_t *second =
        flip_gameswitcher_model_session_at(
            &model,
            1);

    const flip_retained_session_t *third =
        flip_gameswitcher_model_session_at(
            &model,
            2);

    assert(first != NULL);
    assert(second != NULL);
    assert(third != NULL);

    assert(
        strcmp(
            first->rom,
            "/roms/newest.gb") == 0);

    assert(
        strcmp(
            second->rom,
            "/roms/middle.gbc") == 0);

    assert(
        strcmp(
            third->rom,
            "/roms/oldest.gb") == 0);

    flip_gameswitcher_model_free(
        &model);
}


static void test_resumable_session_offers_resume_and_close(void)
{
    fixture_mode = 1;

    flip_gameswitcher_model_t model;

    assert(
        flip_gameswitcher_model_load(
            &model) == 0);

    assert(
        flip_gameswitcher_model_can_resume(
            &model,
            0) == 1);

    assert(
        flip_gameswitcher_model_can_close(
            &model,
            0) == 1);

    flip_gameswitcher_model_free(
        &model);
}

static void test_non_resumable_session_blocks_resume_but_allows_close(void)
{
    fixture_mode = 1;

    flip_gameswitcher_model_t model;

    assert(
        flip_gameswitcher_model_load(
            &model) == 0);

    assert(
        flip_gameswitcher_model_can_resume(
            &model,
            2) == 0);

    assert(
        flip_gameswitcher_model_can_close(
            &model,
            2) == 1);

    flip_gameswitcher_model_free(
        &model);
}


/*
 * This is a headless GameSwitcher model test. Hardware-global
 * controls are tested separately; provide inert linkage here.
 */
void flip_frontend_global_controls_init(
    flip_frontend_global_controls_t *controls)
{
    if (controls)
        memset(controls, 0, sizeof(*controls));
}

int flip_frontend_global_controls_handle(
    flip_frontend_global_controls_t *controls,
    const flip_input_state_t *input,
    flip_frontend_global_controls_result_t *result)
{
    (void)controls;
    (void)input;

    if (!result)
        return -1;

    memset(result, 0, sizeof(*result));
    return 0;
}

int main(void)
{
    test_empty_sessions_offer_new_game();
    test_new_game_does_not_mutate_session_registry();
    test_model_preserves_most_recent_session_order();
    test_resumable_session_offers_resume_and_close();
    test_non_resumable_session_blocks_resume_but_allows_close();
    return 0;
}
