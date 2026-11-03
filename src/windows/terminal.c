#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "../terminal.h"

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#ifndef ENABLE_VIRTUAL_TERMINAL_INPUT
#define ENABLE_VIRTUAL_TERMINAL_INPUT 0x0200
#endif

static DWORD saved_in_mode;
static DWORD saved_out_mode;
static UINT  saved_out_cp;
static int was_termios_saved = 0;
volatile sig_atomic_t running = 1;

/* UTF-8 bytes that didn't fit in the caller's buffer, and a pending high surrogate */
static char    pending[32];
static size_t  pending_len = 0;
static wchar_t high_surrogate = 0;

void handle_signal(int signal) {
    (void)signal;
    running = 0;
}

static BOOL WINAPI ctrl_handler(DWORD type) {
    switch (type) {
    case CTRL_C_EVENT:        /* SIGINT  */
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:    /* SIGTERM */
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        handle_signal(0);
        return TRUE;
    default:
        return FALSE;
    }
}

int term_init(void) {
    SetConsoleCtrlHandler(ctrl_handler, TRUE);

    HANDLE in  = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);

    if (in == INVALID_HANDLE_VALUE || out == INVALID_HANDLE_VALUE)
        return -1;

    if (!GetConsoleMode(in, &saved_in_mode))
        return -1;

    if (!GetConsoleMode(out, &saved_out_mode))
        return -1;

    saved_out_cp = GetConsoleOutputCP();
    was_termios_saved = 1;

    /* ~(ICANON | ECHO); ISIG (PROCESSED_INPUT) stays on so Ctrl+C is a signal.
       VT input makes arrow keys etc. arrive as escape sequences like on Linux. */
    DWORD in_mode = saved_in_mode;
    in_mode &= ~(DWORD)(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
    in_mode |= ENABLE_PROCESSED_INPUT | ENABLE_VIRTUAL_TERMINAL_INPUT;

    if (!SetConsoleMode(in, in_mode)) {
        term_cleanup();
        return -1;
    }

    if (!SetConsoleMode(out, saved_out_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
        term_cleanup();
        return -1;
    }

    SetConsoleOutputCP(CP_UTF8);

    printf("\033[?25l");
    fflush(stdout);

    return 0;
}

void term_cleanup(void) {
    /* Clear first: VT processing must still be enabled for the escape codes. */
    term_print("\033[2J\033[H", 7);

    if (was_termios_saved) {
        SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), saved_in_mode);
        SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), saved_out_mode);
        SetConsoleOutputCP(saved_out_cp);
        was_termios_saved = 0;
    }
}

void term_print(const char *buffer, size_t length) {
    DWORD written;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buffer, (DWORD)length, &written, NULL);
}

/* Appends bytes to the caller's buffer; whatever doesn't fit is kept for next call. */
static void emit(char *buffer, size_t size, size_t *out, const char *bytes, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (*out < size)
            buffer[(*out)++] = bytes[i];
        else if (pending_len < sizeof(pending))
            pending[pending_len++] = bytes[i];
    }
}

/* Returns 1 if there is a real key press waiting (non-key events are discarded). */
ssize_t term_input_available(void) {
    if (pending_len > 0)
        return 1;

    HANDLE in = GetStdHandle(STD_INPUT_HANDLE);

    for (;;) {
        INPUT_RECORD rec;
        DWORD n = 0;

        if (!PeekConsoleInputW(in, &rec, 1, &n) || n == 0)
            return 0;

        if (rec.EventType == KEY_EVENT &&
            rec.Event.KeyEvent.bKeyDown &&
            rec.Event.KeyEvent.uChar.UnicodeChar != 0)
            return 1;

        ReadConsoleInputW(in, &rec, 1, &n);   /* discard mouse/focus/resize/key-up */
    }
}

/* Non-blocking, like VMIN=0 / VTIME=0: returns 0 when nothing is available. */
ssize_t get_input(char *buffer, size_t size) {
    HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
    size_t out = 0;

    size_t take = (pending_len < size) ? pending_len : size;
    memcpy(buffer, pending, take);
    memmove(pending, pending + take, pending_len - take);
    pending_len -= take;
    out = take;

    while (out < size && pending_len == 0 && term_input_available()) {
        INPUT_RECORD rec;
        DWORD n = 0;

        if (!ReadConsoleInputW(in, &rec, 1, &n) || n == 0)
            break;

        if (rec.EventType != KEY_EVENT || !rec.Event.KeyEvent.bKeyDown)
            continue;

        wchar_t wc = rec.Event.KeyEvent.uChar.UnicodeChar;
        if (wc == 0)
            continue;

        wchar_t wide[2];
        int wlen;

        if (wc >= 0xD800 && wc <= 0xDBFF) {          /* high surrogate: wait for low */
            high_surrogate = wc;
            continue;
        } else if (wc >= 0xDC00 && wc <= 0xDFFF && high_surrogate) {
            wide[0] = high_surrogate;
            wide[1] = wc;
            wlen = 2;
            high_surrogate = 0;
        } else {
            wide[0] = wc;
            wlen = 1;
            high_surrogate = 0;
        }

        char utf8[8];
        int blen = WideCharToMultiByte(CP_UTF8, 0, wide, wlen, utf8, sizeof(utf8), NULL, NULL);
        if (blen > 0)
            emit(buffer, size, &out, utf8, (size_t)blen);
    }

    return (ssize_t)out;
}

static int term_size_query(int *cols, int *rows) {
    CONSOLE_SCREEN_BUFFER_INFO info;

    *cols = 0;
    *rows = 0;

    if (!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info))
        return -1;

    *cols = info.srWindow.Right  - info.srWindow.Left + 1;
    *rows = info.srWindow.Bottom - info.srWindow.Top  + 1;
    return 0;
}

int term_get_width(void) {
    int c, r;
    term_size_query(&c, &r);
    return c;
}

int term_get_height(void) {
    int c, r;
    term_size_query(&c, &r);
    return r;
}