#ifndef FLIP_KEYBOARD_H
#define FLIP_KEYBOARD_H

#include <stddef.h>

#define FLIP_KEYBOARD_COLUMNS 10
#define FLIP_KEYBOARD_CHARS \
    "abcdefghijklmnopqrstuvwxyz" \
    "0123456789" \
    "!@#$%^&*()-_=+[]{};:'\",.<>?/\\|`~ "

typedef struct {
    int key_index;
    int uppercase;
    int reveal_text;
} flip_keyboard_t;

void flip_keyboard_init(flip_keyboard_t *keyboard);
int flip_keyboard_key_count(void);
char flip_keyboard_selected_char(const flip_keyboard_t *keyboard);
void flip_keyboard_move(flip_keyboard_t *keyboard, int dx, int dy);
void flip_keyboard_toggle_case(flip_keyboard_t *keyboard);
void flip_keyboard_toggle_reveal(flip_keyboard_t *keyboard);
int flip_keyboard_append_selected(
    const flip_keyboard_t *keyboard,
    char *buffer,
    size_t buffer_size);
int flip_keyboard_backspace(char *buffer);
void flip_keyboard_clear_secret(char *buffer, size_t buffer_size);

#endif
