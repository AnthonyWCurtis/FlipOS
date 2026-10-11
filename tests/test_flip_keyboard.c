#include "flip_keyboard.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    flip_keyboard_t keyboard;
    flip_keyboard_init(&keyboard);
    assert(flip_keyboard_selected_char(&keyboard) == 'a');
    flip_keyboard_toggle_case(&keyboard);
    assert(flip_keyboard_selected_char(&keyboard) == 'A');
    flip_keyboard_move(&keyboard, 1, 0);
    assert(flip_keyboard_selected_char(&keyboard) == 'B');
    flip_keyboard_move(&keyboard, 0, 1);
    assert(flip_keyboard_selected_char(&keyboard) == 'L');

    char buffer[8] = "";
    assert(flip_keyboard_append_selected(&keyboard, buffer, sizeof(buffer)) == 1);
    assert(strcmp(buffer, "L") == 0);
    assert(flip_keyboard_backspace(buffer) == 1);
    assert(buffer[0] == '\0');
    strcpy(buffer, "secret");
    flip_keyboard_clear_secret(buffer, sizeof(buffer));
    for (size_t i = 0; i < sizeof(buffer); ++i)
        assert(buffer[i] == '\0');

    puts("flip_keyboard tests passed");
    return 0;
}
