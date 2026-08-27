#!/usr/bin/env python3

from pathlib import Path

header = Path("include/flip_gameswitcher_ui.h")
source = Path("src/frontend/flip_gameswitcher_ui.c")

if not header.exists():
    print("missing dedicated GameSwitcher UI header")
    raise SystemExit(1)

if not source.exists():
    print("missing dedicated GameSwitcher UI implementation")
    raise SystemExit(1)

header_text = header.read_text()
source_text = source.read_text()

required_header_tokens = [
    "FLIP_GAMESWITCHER_BACK",
    "FLIP_GAMESWITCHER_NEW_GAME",
    "FLIP_GAMESWITCHER_RESUME",
    "FLIP_GAMESWITCHER_CLOSE",
    "FLIP_GAMESWITCHER_ERROR",
    "flip_gameswitcher_result_t",
    "flip_gameswitcher_run(",
]

for token in required_header_tokens:
    if token not in header_text:
        print(f"missing GameSwitcher public interface token: {token}")
        raise SystemExit(1)

if "flip_session_manager_refresh(" not in source_text:
    print("GameSwitcher UI must query Session Manager refresh API")
    raise SystemExit(1)

for token in [
    "recent_roms",
    "load_recent_history",
    "RECENT_FILE",
    "history",
]:
    if token in source_text:
        print(f"GameSwitcher UI depends on Recent data: {token}")
        raise SystemExit(1)

for token in [
    "FLIP_SESSION_MANAGER_ROOT",
    ".session",
    "fnv1a",
    ".sav",
    ".state",
    ".restore-pending",
]:
    if token in source_text:
        print(
            "GameSwitcher UI crosses persistence boundary: "
            f"{token}"
        )
        raise SystemExit(1)



frontend = Path("src/frontend/flip_frontend.c").read_text()

marker = 'FRONTEND_GAMESWITCHER entered'
assert marker in frontend

start = frontend.find(marker)
window_start = max(0, start - 1800)
window_end = min(len(frontend), start + 1800)
gameswitcher_window = frontend[window_start:window_end]

for forbidden in [
    "load_recent_history(",
    "recent_selected",
    "select_suspended_recent(",
    "draw_recent(",
    "SCREEN_RECENT",
]:
    if forbidden in gameswitcher_window:
        print(
            "frontend GameSwitcher still depends on Recent: "
            f"{forbidden}"
        )
        raise SystemExit(1)

if "flip_gameswitcher_run(" not in frontend:
    print(
        "frontend does not delegate GameSwitcher UI "
        "to flip_gameswitcher_run"
    )
    raise SystemExit(1)


interactive_requirements = [
    '#include "flip_draw.h"',
    '#include "flip_input.h"',
    "flip_draw_clear(",
    "flip_draw_text(",
    "flip_drm_present(",
    "flip_input_poll(",
    "FLIP_BTN_UP",
    "FLIP_BTN_DOWN",
    "FLIP_BTN_A",
    "FLIP_BTN_B",
    '"+ NEW GAME"',
]

for token in interactive_requirements:
    if token not in source_text:
        print(
            "GameSwitcher interactive shell missing: "
            f"{token}"
        )
        raise SystemExit(1)


close_action_requirements = [
    "FLIP_BTN_X",
    "FLIP_GAMESWITCHER_CLOSE",
    "X CLOSE",
]

for token in close_action_requirements:
    if token not in source_text:
        print(
            "GameSwitcher close action missing: "
            f"{token}"
        )
        raise SystemExit(1)


# Task 6: selected retained sessions must resume through the
# same generic system/ROM launcher as normal library entries.
resume_marker = "flip_gameswitcher_run("

resume_start = frontend.find(resume_marker)

if resume_start < 0:
    print("GameSwitcher frontend delegation missing")
    raise SystemExit(1)

resume_end = frontend.find(
    "draw_games(",
    resume_start
)

if resume_end < 0:
    print(
        "GameSwitcher frontend resume boundary missing"
    )
    raise SystemExit(1)

resume_window = frontend[
    resume_start:
    resume_end
]

resume_requirements = [
    "FLIP_GAMESWITCHER_RESUME",
    "flip_system_find(",
    "gameswitcher_result.system",
    "gameswitcher_result.rom",
    "launch_game(",
]

for token in resume_requirements:
    if token not in resume_window:
        print(
            "GameSwitcher generic resume path missing: "
            f"{token}"
        )
        raise SystemExit(1)

for forbidden in [
    "flip-gb",
    "gambatte",
]:
    if forbidden.lower() in resume_window.lower():
        print(
            "GameSwitcher resume bypasses generic launcher: "
            f"{forbidden}"
        )
        raise SystemExit(1)


# Task 6: recency changes only on an accepted Resume path.
touch_token = "flip_session_manager_touch("

if touch_token not in resume_window:
    print(
        "GameSwitcher accepted Resume does not touch retained session"
    )
    raise SystemExit(1)

resume_action_pos = resume_window.find(
    "FLIP_GAMESWITCHER_RESUME"
)

system_find_pos = resume_window.find(
    "flip_system_find("
)

touch_pos = resume_window.find(
    touch_token
)

launch_pos = resume_window.find(
    "launch_game("
)

if not (
    0 <= resume_action_pos <
    system_find_pos <
    touch_pos <
    launch_pos
):
    print(
        "GameSwitcher touch must occur only after Resume "
        "acceptance/system resolution and before generic launch"
    )
    raise SystemExit(1)

pre_resume = resume_window[:resume_action_pos]

if touch_token in pre_resume:
    print(
        "GameSwitcher touches session before Resume is accepted"
    )
    raise SystemExit(1)


# Task 6: non-resumable retained sessions must be blocked
# before a RESUME result can reach the frontend launcher.
resume_result_pos = source_text.find(
    "FLIP_GAMESWITCHER_RESUME"
)

resume_guard_pos = source_text.rfind(
    "flip_gameswitcher_model_can_resume(",
    0,
    resume_result_pos
)

if resume_result_pos < 0 or resume_guard_pos < 0:
    print(
        "GameSwitcher Resume is not guarded by resume availability"
    )
    raise SystemExit(1)

resume_guard_window = source_text[
    resume_guard_pos:
    resume_result_pos + len("FLIP_GAMESWITCHER_RESUME")
]

if "selected" not in resume_guard_window:
    print(
        "GameSwitcher Resume availability guard does not "
        "apply to selected session"
    )
    raise SystemExit(1)


# Task 7 stage 1: Close Session must be explicitly confirmed
# before a CLOSE result reaches the frontend.
if "FLIP_GAMESWITCHER_CLOSE" not in frontend:
    print(
        "frontend does not handle GameSwitcher Close result"
    )
    raise SystemExit(1)

for token in [
    "CLOSE SESSION?",
    "A CONFIRM",
    "B CANCEL",
]:
    if token not in source_text:
        print(
            "GameSwitcher close confirmation missing: "
            f"{token}"
        )
        raise SystemExit(1)

# Mutation belongs to the next Task 7 slice.
if "flip_session_manager_remove(" in source_text:
    print(
        "GameSwitcher UI must not remove session metadata directly"
    )
    raise SystemExit(1)


# Task 7 stage 2: confirmed Close may remove only Session Manager
# metadata; save/state/recovery deletion APIs are forbidden.
close_result_pos = frontend.find(
    "FLIP_GAMESWITCHER_CLOSE"
)

close_window = frontend[
    close_result_pos:
    min(len(frontend), close_result_pos + 2200)
]

if "flip_session_manager_remove(" not in close_window:
    print(
        "confirmed GameSwitcher Close does not remove session metadata"
    )
    raise SystemExit(1)

for forbidden in [
    "flip_save_",
    "flip_state_",
    "flip_recovery_",
    ".sav",
    ".state",
    ".prev",
    ".recovery",
    ".restore-pending",
]:
    if forbidden in close_window:
        print(
            "GameSwitcher Close crosses metadata-only boundary: "
            f"{forbidden}"
        )
        raise SystemExit(1)


# Task 7 stage 3: after confirmed Close, the frontend must
# loop back through GameSwitcher so its model refreshes.
close_result_pos = frontend.find(
    "FLIP_GAMESWITCHER_CLOSE"
)

resume_result_pos = frontend.find(
    "FLIP_GAMESWITCHER_RESUME",
    close_result_pos
)

if close_result_pos < 0 or resume_result_pos < 0:
    print(
        "GameSwitcher Close/Resume frontend boundary missing"
    )
    raise SystemExit(1)

close_branch = frontend[
    close_result_pos:
    resume_result_pos
]

remove_pos = close_branch.find(
    "flip_session_manager_remove("
)

continue_pos = close_branch.find(
    "continue;"
)

if not (
    0 <= remove_pos < continue_pos
):
    print(
        "confirmed GameSwitcher Close does not refresh via loop"
    )
    raise SystemExit(1)

run_pos = frontend.rfind(
    "flip_gameswitcher_run(",
    0,
    close_result_pos
)

loop_pos = frontend.rfind(
    "for (;;)",
    0,
    run_pos
)

if loop_pos < 0:
    print(
        "GameSwitcher frontend delegation is not refreshable"
    )
    raise SystemExit(1)


# Task 8: frontend startup must reconcile stale current-session
# metadata before GameSwitcher can display retained sessions.
main_start = frontend.find(
    "int main(void)"
)

if main_start < 0:
    print(
        "frontend main entry missing"
    )
    raise SystemExit(1)

startup_recover = frontend.find(
    "flip_session_manager_recover_interrupted(",
    main_start
)

gameswitcher_entry = frontend.find(
    "run_gameswitcher_frontend(",
    main_start
)

if startup_recover < 0:
    print(
        "frontend startup does not recover interrupted sessions"
    )
    raise SystemExit(1)

if gameswitcher_entry < 0:
    print(
        "GameSwitcher frontend entry missing"
    )
    raise SystemExit(1)

if startup_recover >= gameswitcher_entry:
    print(
        "interrupted-session recovery must run before GameSwitcher display"
    )
    raise SystemExit(1)



# Phase 6 hardware gate: the HOME menu GAMESWITCHER entry must
# invoke the dedicated Session Manager-backed GameSwitcher UI.
#
# It must not route through the legacy Recent-history screen.
home_gameswitcher_marker = "home_selected == 1"

home_gameswitcher_start = frontend.find(
    home_gameswitcher_marker
)

if home_gameswitcher_start < 0:
    print(
        "home-menu GameSwitcher selection branch missing"
    )
    raise SystemExit(1)

home_gameswitcher_end = frontend.find(
    "} else {",
    home_gameswitcher_start
)

if home_gameswitcher_end < 0:
    home_gameswitcher_end = min(
        len(frontend),
        home_gameswitcher_start + 1800
    )

home_gameswitcher_window = frontend[
    home_gameswitcher_start:
    home_gameswitcher_end
]

if "run_gameswitcher_frontend(" not in home_gameswitcher_window:
    print(
        "home-menu GameSwitcher does not delegate "
        "to shared frontend GameSwitcher flow"
    )
    raise SystemExit(1)

shared_flow_start = frontend.find(
    "static flip_gameswitcher_action_t run_gameswitcher_frontend("
)

if shared_flow_start < 0:
    print(
        "shared frontend GameSwitcher flow missing"
    )
    raise SystemExit(1)

shared_flow_end = frontend.find(
    "\n}\n",
    shared_flow_start
)

if shared_flow_end < 0:
    shared_flow_end = min(
        len(frontend),
        shared_flow_start + 7000
    )

shared_flow_window = frontend[
    shared_flow_start:
    shared_flow_end
]

if "flip_gameswitcher_run(" not in shared_flow_window:
    print(
        "shared frontend GameSwitcher flow does not "
        "delegate to flip_gameswitcher_run"
    )
    raise SystemExit(1)

# A game resumed from GameSwitcher may later suspend again when
# MENU is pressed. That SUSPEND must reopen GameSwitcher rather
# than returning the user to the frontend/home screen.
resume_launch_pos = shared_flow_window.find(
    "int resume_result ="
)

suspend_result_pos = shared_flow_window.find(
    "FLIP_SESSION_ACTION_SUSPEND",
    resume_launch_pos
)

if resume_launch_pos < 0:
    print(
        "shared GameSwitcher resume launch result missing"
    )
    raise SystemExit(1)

if suspend_result_pos < 0:
    print(
        "GameSwitcher resume path does not handle "
        "a subsequent game suspend"
    )
    raise SystemExit(1)

resume_suspend_window = shared_flow_window[
    suspend_result_pos:
]

if "continue;" not in resume_suspend_window:
    print(
        "subsequent game suspend does not reopen GameSwitcher"
    )
    raise SystemExit(1)

for forbidden in [
    "load_recent_history(",
    "SCREEN_RECENT",
    "draw_recent(",
]:
    if forbidden in home_gameswitcher_window:
        print(
            "home-menu GameSwitcher still routes "
            f"through Recent: {forbidden}"
        )
        raise SystemExit(1)


# Task 9: Recent history remains a separate feature and must
# not retain GameSwitcher session-storage naming.
recent_start = frontend.find(
    "static int load_recent_history(void)"
)

recent_end = frontend.find(
    "static int",
    recent_start + 1
)

if recent_start < 0:
    print(
        "Recent history loader missing"
    )
    raise SystemExit(1)

if recent_end < 0:
    recent_end = len(frontend)

recent_window = frontend[
    recent_start:
    recent_end
]

for forbidden in [
    "GameSwitcher sessions",
    "GAMESWITCHER_CONFIRM_",
    "gameswitcher_confirm",
]:
    if forbidden in recent_window:
        print(
            "Recent history still carries GameSwitcher "
            f"session naming: {forbidden}"
        )
        raise SystemExit(1)

# The dedicated GameSwitcher UI must never parse or mutate
# Recent/history storage.
for forbidden in [
    "/state/recent/history",
    "load_recent_history(",
    "flip_session_prune_recent(",
    "flip_session_remove_recent(",
]:
    if forbidden in source_text:
        print(
            "GameSwitcher depends on Recent history: "
            f"{forbidden}"
        )
        raise SystemExit(1)

print("gameswitcher architecture boundary passed")
