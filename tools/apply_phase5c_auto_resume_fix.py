#!/usr/bin/env python3
from pathlib import Path


def replace_once(path: str, old: str, new: str) -> None:
    file = Path(path)
    text = file.read_text()
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{path}: expected one anchor, found {count}")
    file.write_text(text.replace(old, new, 1))


replace_once(
    "include/flip_session.h",
    "typedef struct {\n    const flip_system_t *system;\n    const char *rom_path;\n} flip_session_metadata_t;",
    "typedef struct {\n    const flip_system_t *system;\n    const char *rom_path;\n\n    /*\n     * Non-zero only when the frontend is automatically continuing the\n     * same session after a lid wake. Deliberate launches remain interactive.\n     */\n    int automatic_resume;\n} flip_session_metadata_t;",
)

replace_once(
    "src/frontend/flip_frontend.c",
    "static int launch_game_once(\n    flip_drm_t *drm,\n    const rom_entry_t *rom)",
    "static int launch_game_once(\n    flip_drm_t *drm,\n    const rom_entry_t *rom,\n    int automatic_resume)",
)

replace_once(
    "src/frontend/flip_frontend.c",
    "    flip_session_metadata_t metadata = {\n        .system = system,\n        .rom_path = rom->path\n    };",
    "    flip_session_metadata_t metadata = {\n        .system = system,\n        .rom_path = rom->path,\n        .automatic_resume = automatic_resume\n    };",
)

replace_once(
    "src/frontend/flip_frontend.c",
    "static int launch_game(\n    flip_drm_t *drm,\n    const rom_entry_t *rom)\n{\n    for (;;) {\n\n        int result =\n            launch_game_once(\n                drm,\n                rom);",
    "static int launch_game(\n    flip_drm_t *drm,\n    const rom_entry_t *rom)\n{\n    int automatic_resume = 0;\n\n    for (;;) {\n\n        int result =\n            launch_game_once(\n                drm,\n                rom,\n                automatic_resume);\n\n        /* The intent applies to exactly one post-wake relaunch. */\n        automatic_resume = 0;",
)

replace_once(
    "src/frontend/flip_frontend.c",
    "        fprintf(\n            stderr,\n            \"FRONTEND_STANDBY auto resume rom=%s\\n\",\n            rom->path);\n\n        /*\n         * Loop back through the normal session launcher.\n         * Gambatte will restore the saved per-ROM state.\n         */",
    "        fprintf(\n            stderr,\n            \"FRONTEND_STANDBY auto resume rom=%s\\n\",\n            rom->path);\n\n        /*\n         * This next launch is a continuation of the lid-suspended session,\n         * not a new user launch decision. Skip Game Options exactly once.\n         */\n        automatic_resume = 1;\n\n        /*\n         * Loop back through the normal session launcher.\n         * Gambatte will restore the saved per-ROM state.\n         */",
)

replace_once(
    "src/frontend/flip_save_safety_launch.c",
    "    int choice = choose_launch_mode(\n        drm,\n        metadata->system,\n        metadata->rom_path);",
    "    if (metadata->automatic_resume) {\n        fprintf(\n            stderr,\n            \"FLIP_SAVE_SAFETY: AUTOMATIC_RESUME\\n\");\n\n        return flip_session_launch_legacy(\n            drm,\n            program,\n            argv,\n            metadata,\n            result);\n    }\n\n    int choice = choose_launch_mode(\n        drm,\n        metadata->system,\n        metadata->rom_path);",
)

# This helper exists only to safely bridge the connector's whole-file update
# limitation for the large frontend source. Do not keep it in the product tree.
Path(__file__).unlink()

print("Phase 5C automatic lid-resume fix applied")
