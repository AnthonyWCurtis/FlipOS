#include "flip_gameswitcher_ui.h"
#include "flip_frontend_present.h"
#include "flip_frontend_global_controls.h"
#include "flip_frontend_idle.h"
#include "flip_lifecycle.h"
#include "flip_draw.h"
#include "flip_input.h"
#include "flip_session_manager.h"

#include <stdio.h>
#include <string.h>

#define GS_BG       0x00101010
#define GS_FG       0x00ffffff
#define GS_DIM      0x00505050
#define GS_PANEL    0x00202020

#define GS_ROWS_VISIBLE 9

static int centred_text_x(
    const char *text,
    int scale)
{
    size_t len =
        text ? strlen(text) : 0;

    int width =
        (int)len * 6 * scale;

    return (640 - width) / 2;
}

int flip_gameswitcher_model_load(
    flip_gameswitcher_model_t *model)
{
    if (!model)
        return -1;

    memset(
        model,
        0,
        sizeof(*model));

    return flip_session_manager_refresh(
        &model->sessions);
}

size_t flip_gameswitcher_model_count(
    const flip_gameswitcher_model_t *model)
{
    if (!model)
        return 0;

    return model->sessions.count;
}

const flip_retained_session_t *
flip_gameswitcher_model_session_at(
    const flip_gameswitcher_model_t *model,
    size_t index)
{
    if (!model ||
        index >= model->sessions.count)
        return NULL;

    return &model->sessions.items[index];
}

void flip_gameswitcher_model_free(
    flip_gameswitcher_model_t *model)
{
    if (!model)
        return;

    flip_session_manager_list_free(
        &model->sessions);

    memset(
        model,
        0,
        sizeof(*model));
}

int flip_gameswitcher_model_can_resume(
    const flip_gameswitcher_model_t *model,
    size_t index)
{
    const flip_retained_session_t *session =
        flip_gameswitcher_model_session_at(
            model,
            index);

    if (!session)
        return 0;

    return session->resume_available != 0;
}

int flip_gameswitcher_model_can_close(
    const flip_gameswitcher_model_t *model,
    size_t index)
{
    return flip_gameswitcher_model_session_at(
        model,
        index) != NULL;
}

static void draw_gameswitcher(
    flip_drm_t *drm,
    const flip_gameswitcher_model_t *model,
    size_t selected,
    int confirm_close)
{
    size_t count =
        flip_gameswitcher_model_count(
            model);

    size_t item_count =
        count + 1;

    flip_draw_clear(
        drm,
        GS_BG);

    flip_draw_text(
        drm,
        centred_text_x(
            "GAMESWITCHER",
            3),
        20,
        "GAMESWITCHER",
        3,
        GS_FG);

    char count_text[64];

    snprintf(
        count_text,
        sizeof(count_text),
        "%zu SESSIONS",
        count);

    flip_draw_text(
        drm,
        centred_text_x(
            count_text,
            1),
        61,
        count_text,
        1,
        GS_DIM);

    size_t first = 0;

    if (selected >
        GS_ROWS_VISIBLE / 2) {

        first =
            selected -
            GS_ROWS_VISIBLE / 2;
    }

    if (item_count >
            GS_ROWS_VISIBLE &&
        first >
            item_count -
            GS_ROWS_VISIBLE) {

        first =
            item_count -
            GS_ROWS_VISIBLE;
    }

    for (size_t row = 0;
         row < GS_ROWS_VISIBLE;
         ++row) {

        size_t index =
            first + row;

        if (index >= item_count)
            break;

        int y =
            92 + (int)row * 34;

        if (index == selected) {

            flip_draw_rect(
                drm,
                55,
                y - 5,
                530,
                29,
                GS_PANEL);

            flip_draw_rect_outline(
                drm,
                55,
                y - 5,
                530,
                29,
                1,
                GS_FG);
        }

        if (index == count) {

            flip_draw_text(
                drm,
                72,
                y,
                "+ NEW GAME",
                2,
                index == selected
                    ? GS_FG
                    : GS_DIM);

            continue;
        }

        const flip_retained_session_t *session =
            flip_gameswitcher_model_session_at(
                model,
                index);

        if (!session)
            continue;

        const char *name =
            strrchr(
                session->rom,
                '/');

        name =
            name
                ? name + 1
                : session->rom;

        char visible[43];

        snprintf(
            visible,
            sizeof(visible),
            "%.40s",
            name);

        flip_draw_text(
            drm,
            72,
            y,
            visible,
            2,
            index == selected
                ? GS_FG
                : GS_DIM);

        const char *status =
            session->resume_available
                ? "RESUME"
                : "SESSION";

        flip_draw_text(
            drm,
            session->resume_available
                ? 500
                : 494,
            y + 4,
            status,
            1,
            index == selected
                ? GS_FG
                : GS_DIM);
    }

    const char *help =
        selected == count
            ? "A NEW GAME   B BACK"
            : flip_gameswitcher_model_can_resume(
                  model,
                  selected)
                ? "A RESUME   X CLOSE   B BACK"
                : "X CLOSE   B BACK";

    flip_draw_text(
        drm,
        centred_text_x(
            help,
            1),
        452,
        help,
        1,
        GS_DIM);

    if (confirm_close) {

        flip_draw_rect(
            drm,
            115,
            365,
            410,
            76,
            GS_PANEL);

        flip_draw_rect_outline(
            drm,
            115,
            365,
            410,
            76,
            1,
            GS_FG);

        flip_draw_text(
            drm,
            centred_text_x(
                "CLOSE SESSION?",
                2),
            382,
            "CLOSE SESSION?",
            2,
            GS_FG);

        flip_draw_text(
            drm,
            centred_text_x(
                "A CONFIRM   B CANCEL",
                1),
            418,
            "A CONFIRM   B CANCEL",
            1,
            GS_DIM);
    }

    flip_frontend_present(
        drm);
}

static int set_selected_result(
    flip_gameswitcher_result_t *result,
    flip_gameswitcher_action_t action,
    const flip_retained_session_t *session)
{
    if (!result)
        return -1;

    result->action =
        action;

    if (!session)
        return 0;

    snprintf(
        result->system,
        sizeof(result->system),
        "%s",
        session->system);

    snprintf(
        result->rom,
        sizeof(result->rom),
        "%s",
        session->rom);

    return 0;
}

int flip_gameswitcher_run(
    flip_drm_t *drm,
    flip_gameswitcher_result_t *result)
{
    if (!result)
        return -1;

    memset(
        result,
        0,
        sizeof(*result));

    result->action =
        FLIP_GAMESWITCHER_ERROR;

    flip_gameswitcher_model_t model;

    if (flip_gameswitcher_model_load(
            &model) < 0)
        return -1;

    size_t count =
        flip_gameswitcher_model_count(
            &model);

    /*
     * Headless host-model path.
     *
     * Real frontend callers always provide DRM. Keeping this
     * deterministic path lets the pure model tests exercise
     * empty-session behaviour without hardware dependencies.
     */
    if (!drm) {

        flip_gameswitcher_model_free(
            &model);

        result->action =
            count == 0
                ? FLIP_GAMESWITCHER_NEW_GAME
                : FLIP_GAMESWITCHER_BACK;

        return 0;
    }

    size_t selected = 0;
    int confirm_close = 0;

    flip_input_state_t state;

    memset(
        &state,
        0,
        sizeof(state));

    int prev_up = 0;
    int prev_down = 0;
    int prev_a = 0;
    int prev_b = 0;
    int prev_x = 0;

    flip_frontend_global_controls_t
        global_controls;

    flip_frontend_global_controls_init(
        &global_controls);

    draw_gameswitcher(
        drm,
        &model,
        selected,
        confirm_close);

    for (;;) {

        if (flip_input_poll(
                &state,
                30) < 0) {

            flip_gameswitcher_model_free(
                &model);

            result->action =
                FLIP_GAMESWITCHER_ERROR;

            return -1;
        }

        int idle_input_consumed =
            flip_frontend_idle_global_process_input(
                &state);

        if (idle_input_consumed < 0) {
            fprintf(
                stderr,
                "GAMESWITCHER: idle input handling failed\n");
        }

        int up =
            state.down[
                FLIP_BTN_UP];

        int down =
            state.down[
                FLIP_BTN_DOWN];

        int a =
            state.down[
                FLIP_BTN_A];

        int b =
            state.down[
                FLIP_BTN_B];

        int x =
            state.down[
                FLIP_BTN_X];

        int redraw = 0;

        if (flip_frontend_idle_global_redraw_due() ||
            idle_input_consumed > 0) {

            redraw = 1;
        }

        flip_frontend_global_controls_result_t
            global_result;

        if (flip_frontend_global_controls_handle(
                &global_controls,
                &state,
                &global_result) < 0) {

            fprintf(
                stderr,
                "GAMESWITCHER: global controls failed\n");
        }

        if (global_result.redraw)
            redraw = 1;

        if (global_result.shutdown_requested) {

            /*
             * Successful power-off does not return. If reboot()
             * fails, remain in GameSwitcher rather than pretending
             * the user requested a navigation action.
             */
            redraw = 1;
        }

        if (up &&
            !prev_up) {

            if (selected == 0)
                selected = count;
            else
                selected--;

            redraw = 1;
        }

        if (down &&
            !prev_down) {

            selected++;

            if (selected > count)
                selected = 0;

            redraw = 1;
        }

        if (b &&
            !prev_b) {

            if (confirm_close) {

                confirm_close = 0;
                redraw = 1;

            } else {

                flip_gameswitcher_model_free(
                    &model);

                result->action =
                    FLIP_GAMESWITCHER_BACK;

                return 0;
            }
        }

        if (x &&
            !prev_x &&
            !confirm_close &&
            selected < count &&
            flip_gameswitcher_model_can_close(
                &model,
                selected)) {

            confirm_close = 1;
            redraw = 1;
        }

        if (a &&
            !prev_a &&
            confirm_close) {

            const flip_retained_session_t *session =
                flip_gameswitcher_model_session_at(
                    &model,
                    selected);

            set_selected_result(
                result,
                FLIP_GAMESWITCHER_CLOSE,
                session);

            flip_gameswitcher_model_free(
                &model);

            return 0;
        }

        if (a &&
            !prev_a) {

            if (selected == count) {

                flip_gameswitcher_model_free(
                    &model);

                result->action =
                    FLIP_GAMESWITCHER_NEW_GAME;

                return 0;
            }

            if (flip_gameswitcher_model_can_resume(
                    &model,
                    selected)) {

                const flip_retained_session_t *session =
                    flip_gameswitcher_model_session_at(
                        &model,
                        selected);

                set_selected_result(
                    result,
                    FLIP_GAMESWITCHER_RESUME,
                    session);

                flip_gameswitcher_model_free(
                    &model);

                return 0;
            }
        }

        if (redraw) {

            draw_gameswitcher(
                drm,
                &model,
                selected,
                confirm_close);
        }

        prev_up = up;
        prev_down = down;
        prev_a = a;
        prev_b = b;
        prev_x = x;
    }
}
