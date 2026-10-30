#include <stddef.h>
#include <sys/types.h>
#include "../terminal.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>


static struct termios saved_termios;
static int was_termios_saved = 0;


int term_init(void) {
    struct termios termios;

    if (tcgetattr(STDIN_FILENO, &saved_termios) == -1)
        return -1;

    was_termios_saved = 1;
    termios = saved_termios;

    termios.c_lflag &= ~(ICANON | ECHO);
    termios.c_cc[VMIN] = 0;
    termios.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &termios) == -1) {
        term_cleanup();
        return -1;
    }

    printf("\033[?25l");
    fflush(stdout);

    return 0;
}

void term_cleanup(void) {
    if (was_termios_saved) {
        tcsetattr(STDIN_FILENO, TCSANOW, &saved_termios);
        was_termios_saved = 0;
    }
}

void term_print(const char *buffer, size_t length) {
    write(STDOUT_FILENO, buffer, length);
}

ssize_t get_input(char *buffer, size_t size) {
    return (ssize_t)read(STDIN_FILENO, buffer, size);
}

static struct winsize term_size(void) {
    struct winsize ws = {0};

    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);

    return ws;
}

int term_get_width(void) {
    return term_size().ws_col;
}

int term_get_height(void) {
    return term_size().ws_row;
}