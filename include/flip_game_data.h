#ifndef FLIP_GAME_DATA_H
#define FLIP_GAME_DATA_H

/*
 * Recoverably reset all FlipOS-managed persistence for one game.
 *
 * The implementation owns persistence paths and must not require
 * frontend knowledge of save/state/recovery/session filenames.
 *
 * Returns:
 *   0  reset completed successfully
 *  -1  invalid input or reset failure
 */
int flip_game_data_reset(
    const char *system_id,
    const char *rom_path);

#endif
