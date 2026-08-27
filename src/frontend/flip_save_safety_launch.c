#include "flip_save_safety_ui.h"

#include "flip_draw.h"
#include "flip_input.h"
#include "flip_session.h"
#include "flip_system.h"

#include <stdio.h>
#include <string.h>

#define BG    0x00101010
#define FG    0x00ffffff
#define DIM   0x00505050
#define PANEL 0x00202020

int flip_session_launch_legacy(
    flip_drm_t *drm,
    const char *program,
    char *const argv[],
    const flip_session_metadata_t *metadata,
    flip_session_result_t *result);

static int centred_text_x(const char *text, int scale)
{
    int width = text ? (int)strlen(text) * 6 * scale : 0;
    return (640 - width) / 2;
}

static void draw_row(
    flip_drm_t *drm,
    const char *text,
    int y,
    int selected)
{
    if (selected) {
        flip_draw_rect(drm, 95, y - 8, 450, 38, PANEL);
        flip_draw_rect_outline(drm, 95, y - 8, 450, 38, 1, FG);
    }

    flip_draw_text(
        drm,
        centred_text_x(text, 2),
        y,
        text,
        2,
        selected ? FG : DIM);
}

static void draw_action_menu(
    flip_drm_t *drm,
    int selected,
    int can_resume)
{
    flip_draw_clear(drm, BG);
    flip_draw_text(
        drm,
        centred_text_x("GAME OPTIONS", 4),
        38,
        "GAME OPTIONS",
        4,
        FG);

    int row = 0;
    if (can_resume) {
        draw_row(drm, "RESUME GAME", 145 + row * 58, selected == row);
        row++;
    }

    draw_row(drm, "START NORMALLY", 145 + row * 58, selected == row);
    row++;
    draw_row(drm, "SAVE SAFETY", 145 + row * 58, selected == row);

    flip_draw_text(
        drm,
        centred_text_x("A SELECT   B BACK", 1),
        442,
        "A SELECT   B BACK",
        1,
        DIM);
    flip_drm_present(drm);
}

static const char *undo_state_name(
    flip_save_safety_health_t health)
{
    if (health == FLIP_SAVE_SAFETY_HEALTH_HEALTHY)
        return "AVAILABLE";
    return flip_save_safety_health_name(health);
}

static void draw_status_line(
    flip_drm_t *drm,
    int y,
    const char *label,
    const char *value)
{
    char line[96];
    snprintf(line, sizeof(line), "%s: %s", label, value);
    flip_draw_text(drm, 120, y, line, 1, FG);
}

static void draw_save_safety(
    flip_drm_t *drm,
    const flip_save_safety_status_t *status,
    int status_ok,
    int selected,
    const char *message)
{
    flip_draw_clear(drm, BG);
    flip_draw_text(
        drm,
        centred_text_x("SAVE SAFETY", 4),
        24,
        "SAVE SAFETY",
        4,
        FG);

    if (status_ok) {
        draw_status_line(drm, 85, "CURRENT", flip_save_safety_health_name(status->current));
        draw_status_line(drm, 105, "PREVIOUS", flip_save_safety_health_name(status->previous));
        draw_status_line(drm, 125, "RECOVERY", flip_save_safety_health_name(status->recovery));
        draw_status_line(drm, 145, "RESUME", status->resume_blocked ? "BLOCKED" : "AVAILABLE");
        draw_status_line(drm, 165, "UNDO STATE", undo_state_name(status->undo_state));
    } else {
        flip_draw_text(
            drm,
            centred_text_x("STATUS UNAVAILABLE", 2),
            112,
            "STATUS UNAVAILABLE",
            2,
            FG);
    }

    draw_row(drm, "RESTORE PREVIOUS SAVE", 230, selected == 0);
    draw_row(drm, "UNDO LAST RESTORE", 282, selected == 1);
    draw_row(drm, "BACK", 334, selected == 2);

    if (message && message[0]) {
        flip_draw_text(
            drm,
            centred_text_x(message, 1),
            392,
            message,
            1,
            FG);
    }

    flip_draw_text(
        drm,
        centred_text_x("A SELECT   B BACK", 1),
        442,
        "A SELECT   B BACK",
        1,
        DIM);
    flip_drm_present(drm);
}

static void draw_confirmation(
    flip_drm_t *drm,
    const char *title)
{
    flip_draw_clear(drm, BG);
    flip_draw_text(
        drm,
        centred_text_x("SAVE SAFETY", 4),
        58,
        "SAVE SAFETY",
        4,
        FG);
    flip_draw_rect(drm, 70, 170, 500, 130, PANEL);
    flip_draw_rect_outline(drm, 70, 170, 500, 130, 1, FG);
    flip_draw_text(
        drm,
        centred_text_x(title, 2),
        205,
        title,
        2,
        FG);
    flip_draw_text(
        drm,
        centred_text_x("A CONFIRM   B CANCEL", 1),
        260,
        "A CONFIRM   B CANCEL",
        1,
        DIM);
    flip_drm_present(drm);
}

static int poll_edge(
    flip_input_state_t *state,
    flip_button_t button,
    int *previous)
{
    int down = state->down[button] != 0;
    int edge = down && !*previous;
    *previous = down;
    return edge;
}

static void wait_for_action_buttons_release(void)
{
    flip_input_state_t state;
    memset(&state, 0, sizeof(state));

    for (;;) {
        if (flip_input_poll(&state, 20) < 0)
            return;

        if (!state.down[FLIP_BTN_A] &&
            !state.down[FLIP_BTN_B] &&
            !state.down[FLIP_BTN_UP] &&
            !state.down[FLIP_BTN_DOWN]) {
            return;
        }
    }
}

static int confirm_action(flip_drm_t *drm, const char *title)
{
    wait_for_action_buttons_release();
    draw_confirmation(drm, title);

    flip_input_state_t state;
    memset(&state, 0, sizeof(state));
    int prev_a = 0;
    int prev_b = 0;

    for (;;) {
        if (flip_input_poll(&state, 30) < 0)
            return 0;

        if (poll_edge(&state, FLIP_BTN_A, &prev_a))
            return 1;
        if (poll_edge(&state, FLIP_BTN_B, &prev_b))
            return 0;
    }
}

static int run_save_safety_menu(
    flip_drm_t *drm,
    const flip_system_t *system,
    const char *rom_path)
{
    int selected = 0;
    char message[48] = "";
    flip_save_safety_status_t status;
    int status_ok = flip_save_safety_query(system, rom_path, &status) == 0;

    if (status_ok) {
        fprintf(
            stderr,
            "FLIP_SAVE_SAFETY: current=%s previous=%s recovery=%s resume=%s undo_state=%s\n",
            flip_save_safety_health_name(status.current),
            flip_save_safety_health_name(status.previous),
            flip_save_safety_health_name(status.recovery),
            status.resume_blocked ? "BLOCKED" : "AVAILABLE",
            undo_state_name(status.undo_state));
    }

    wait_for_action_buttons_release();
    draw_save_safety(drm, &status, status_ok, selected, message);

    flip_input_state_t state;
    memset(&state, 0, sizeof(state));
    int prev_up = 0;
    int prev_down = 0;
    int prev_a = 0;
    int prev_b = 0;

    for (;;) {
        if (flip_input_poll(&state, 30) < 0)
            return -1;

        int redraw = 0;

        if (poll_edge(&state, FLIP_BTN_UP, &prev_up)) {
            selected = (selected + 2) % 3;
            redraw = 1;
        }
        if (poll_edge(&state, FLIP_BTN_DOWN, &prev_down)) {
            selected = (selected + 1) % 3;
            redraw = 1;
        }
        if (poll_edge(&state, FLIP_BTN_B, &prev_b))
            return 0;

        if (poll_edge(&state, FLIP_BTN_A, &prev_a)) {
            if (selected == 2)
                return 0;

            const int restore = selected == 0;
            const char *confirm = restore
                ? "RESTORE PREVIOUS SAVE?"
                : "UNDO LAST RESTORE?";

            if (!confirm_action(drm, confirm)) {
                wait_for_action_buttons_release();
                memset(&state, 0, sizeof(state));
                prev_up = prev_down = prev_a = prev_b = 0;
                redraw = 1;
            } else {
                int action_result = restore
                    ? flip_save_safety_restore_previous(system, rom_path)
                    : flip_save_safety_undo(system, rom_path);

                if (action_result == 0) {
                    snprintf(
                        message,
                        sizeof(message),
                        "%s",
                        restore ? "RESTORE COMPLETE" : "UNDO COMPLETE");
                } else if (action_result > 0) {
                    snprintf(message, sizeof(message), "NOT AVAILABLE");
                } else {
                    snprintf(message, sizeof(message), "ACTION FAILED");
                }

                status_ok = flip_save_safety_query(system, rom_path, &status) == 0;
                wait_for_action_buttons_release();
                memset(&state, 0, sizeof(state));
                prev_up = prev_down = prev_a = prev_b = 0;
                redraw = 1;
            }
        }

        if (redraw)
            draw_save_safety(drm, &status, status_ok, selected, message);
    }
}

static int choose_launch_mode(
    flip_drm_t *drm,
    const flip_system_t *system,
    const char *rom_path)
{
    const int can_resume = flip_system_resume_available(system, rom_path);
    const int count = can_resume ? 3 : 2;
    int selected = 0;

    wait_for_action_buttons_release();
    draw_action_menu(drm, selected, can_resume);

    flip_input_state_t state;
    memset(&state, 0, sizeof(state));
    int prev_up = 0;
    int prev_down = 0;
    int prev_a = 0;
    int prev_b = 0;

    for (;;) {
        if (flip_input_poll(&state, 30) < 0)
            return -1;

        int redraw = 0;

        if (poll_edge(&state, FLIP_BTN_UP, &prev_up)) {
            selected = (selected + count - 1) % count;
            redraw = 1;
        }
        if (poll_edge(&state, FLIP_BTN_DOWN, &prev_down)) {
            selected = (selected + 1) % count;
            redraw = 1;
        }
        if (poll_edge(&state, FLIP_BTN_B, &prev_b))
            return 0;

        if (poll_edge(&state, FLIP_BTN_A, &prev_a)) {
            if (can_resume && selected == 0)
                return 1;

            int start_index = can_resume ? 1 : 0;
            int safety_index = can_resume ? 2 : 1;

            if (selected == start_index)
                return 2;

            if (selected == safety_index) {
                if (run_save_safety_menu(drm, system, rom_path) < 0)
                    return -1;
                wait_for_action_buttons_release();
                memset(&state, 0, sizeof(state));
                prev_up = prev_down = prev_a = prev_b = 0;
                redraw = 1;
            }
        }

        if (redraw)
            draw_action_menu(drm, selected, can_resume);
    }
}

int flip_session_launch(
    flip_drm_t *drm,
    const char *program,
    char *const argv[],
    const flip_session_metadata_t *metadata,
    flip_session_result_t *result)
{
    if (!metadata ||
        !metadata->system ||
        !metadata->rom_path ||
        !flip_save_safety_supported(metadata->system)) {
        return flip_session_launch_legacy(
            drm,
            program,
            argv,
            metadata,
            result);
    }

    if (metadata->automatic_resume ||
        metadata->gameswitcher_resume) {

        fprintf(
            stderr,
            metadata->automatic_resume
                ? "FLIP_SAVE_SAFETY: AUTOMATIC_RESUME\n"
                : "FLIP_SAVE_SAFETY: GAMESWITCHER_RESUME\n");

        return flip_session_launch_legacy(
            drm,
            program,
            argv,
            metadata,
            result);
    }

    int choice = choose_launch_mode(
        drm,
        metadata->system,
        metadata->rom_path);

    if (choice < 0)
        return -1;

    if (choice == 0) {
        if (result) {
            memset(result, 0, sizeof(*result));
            result->action = FLIP_SESSION_ACTION_EXIT;
        }
        return 0;
    }

    if (choice == 1) {
        return flip_session_launch_legacy(
            drm,
            program,
            argv,
            metadata,
            result);
    }

    char *normal_argv[5] = {0};
    if (flip_save_safety_build_start_normal(
            metadata->system,
            metadata->rom_path,
            normal_argv,
            sizeof(normal_argv) / sizeof(normal_argv[0])) < 0) {
        return -1;
    }

    return flip_session_launch_legacy(
        drm,
        program,
        normal_argv,
        metadata,
        result);
}
