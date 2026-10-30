#include "display.h"
#include "terminal.h"
#include <string.h>
#include <unistd.h>


char display_buffer[65536];
size_t display_pos = 0;
char input_buffer[255] = {0};
char chat_buffer[1024][255];
int chat_count = 0;


void append_char(char c) {
    display_buffer[display_pos++] = c;
}

void append_char_repeat(char c, size_t count) {
    memset(display_buffer + display_pos, (unsigned char)c, count);
    display_pos += count;
}

void append_string(const char *str) {
    size_t length = strlen(str);

    memcpy(display_buffer + display_pos, str, length);
    display_pos += length;
}

void append_string_repeat(const char *str, size_t count) {
    size_t len = strlen(str);

    for (size_t i = 0; i < count; i++) {
        memcpy(display_buffer + display_pos, str, len);
        display_pos += len;
    }
}

void display() {
    int pad = term_get_width();
    append_string("\033[2J\033[H");
    append_string(" ╭");
    append_string_repeat("─", pad-4);
    append_string("╮");
    append_char(10);

    for (int i = 0; i < chat_count; i++) {
        append_string(" │  ");
        append_string(chat_buffer[i]);
        append_char_repeat(' ', pad-strlen(chat_buffer[i])+7);
        append_string("│");
        append_char(10);
    }

    for (int i = term_get_height()-4-chat_count; i > 0; i--) {
        append_string(" │");
        append_char_repeat(' ', pad - 4);
        append_string("│");
        append_char(10);
    }

    append_string(" ├");
    append_string_repeat("─", pad - 4);
    append_string("┤");
    append_char(10);

    append_string(" │  \033[1m>\033[0m ");
    append_string(input_buffer);
    append_char_repeat(' ', pad - strlen(input_buffer) - 9);
    append_string(" │");

    append_char(10);
    append_string(" ╰");
    append_string_repeat("─", pad-4);
    append_string("╯");

    term_print(display_buffer, display_pos);
    display_pos = 0;
}

void print_message(const char *str) {
    strcat(chat_buffer[chat_count], str);
    chat_count++;
    display();
}
