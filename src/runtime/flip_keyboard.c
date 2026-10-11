#include "flip_keyboard.h"

#include <string.h>

static const char keyboard_chars[] = FLIP_KEYBOARD_CHARS;

void flip_keyboard_init(flip_keyboard_t *keyboard)
{
    if (!keyboard)
        return;
    keyboard->key_index = 0;
    keyboard->uppercase = 0;
    keyboard->reveal_text = 0;
}

int flip_keyboard_key_count(void)
{
    return (int)(sizeof(keyboard_chars) - 1);
}

char flip_keyboard_selected_char(const flip_keyboard_t *keyboard)
{
    if (!keyboard)
        return '\0';

    int count = flip_keyboard_key_count();
    if (count <= 0)
        return '\0';

    int index = keyboard->key_index % count;
    if (index < 0)
        index += count;

    char key = keyboard_chars[index];
    if (keyboard->uppercase && key >= 'a' && key <= 'z')
        key = (char)(key - 'a' + 'A');
    return key;
}

void flip_keyboard_move(flip_keyboard_t *keyboard, int dx, int dy)
{
    if (!keyboard)
        return;

    int count = flip_keyboard_key_count();
    if (count <= 0)
        return;

    int index = keyboard->key_index + dx + dy * FLIP_KEYBOARD_COLUMNS;
    while (index < 0)
        index += count;
    while (index >= count)
        index -= count;
    keyboard->key_index = index;
}

void flip_keyboard_toggle_case(flip_keyboard_t *keyboard)
{
    if (keyboard)
        keyboard->uppercase = !keyboard->uppercase;
}

void flip_keyboard_toggle_reveal(flip_keyboard_t *keyboard)
{
    if (keyboard)
        keyboard->reveal_text = !keyboard->reveal_text;
}

int flip_keyboard_append_selected(
    const flip_keyboard_t *keyboard,
    char *buffer,
    size_t buffer_size)
{
    if (!keyboard || !buffer || buffer_size == 0)
        return -1;

    size_t length = strlen(buffer);
    if (length + 1 >= buffer_size)
        return 0;

    char key = flip_keyboard_selected_char(keyboard);
    if (!key)
        return -1;

    buffer[length] = key;
    buffer[length + 1] = '\0';
    return 1;
}

int flip_keyboard_backspace(char *buffer)
{
    if (!buffer)
        return -1;

    size_t length = strlen(buffer);
    if (length == 0)
        return 0;

    buffer[length - 1] = '\0';
    return 1;
}

void flip_keyboard_clear_secret(char *buffer, size_t buffer_size)
{
    if (!buffer)
        return;

    volatile char *wipe = (volatile char *)buffer;
    for (size_t i = 0; i < buffer_size; ++i)
        wipe[i] = '\0';
}
