#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include "../wait.h"
#include "../terminal.h"   /* needs: int term_input_available(void); */

#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#endif

static int socket_readable(socket_t sockfd) {
    fd_set set;
    struct timeval tv = {0, 0};

    FD_ZERO(&set);
    FD_SET(sockfd, &set);

    int r = select(0, NULL == NULL ? &set : NULL, NULL, NULL, &tv);
    return r > 0;
}

int poll_wait(socket_t sockfd, int timeout_ms) {
    HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
    WSAEVENT ev = WSACreateEvent();

    if (ev == WSA_INVALID_EVENT)
        return -1;

    /* Note: WSAEventSelect puts the socket in non-blocking mode; undone below. */
    if (WSAEventSelect(sockfd, ev, FD_READ) == SOCKET_ERROR) {
        WSACloseEvent(ev);
        return -1;
    }

    HANDLE handles[2] = { in, (HANDLE)ev };
    ULONGLONG deadline = (timeout_ms < 0) ? 0 : GetTickCount64() + (ULONGLONG)timeout_ms;
    int ready = 0;
    int result = 0;

    for (;;) {
        ready = 0;

        if (term_input_available())
            ready |= POLL_INPUT;

        if (socket_readable(sockfd))
            ready |= POLL_SOCKET;

        if (ready) {
            result = ready;
            break;
        }

        DWORD wait_ms;
        if (timeout_ms < 0) {
            wait_ms = INFINITE;
        } else {
            ULONGLONG now = GetTickCount64();
            if (now >= deadline) {
                result = 0;
                break;
            }
            wait_ms = (DWORD)(deadline - now);
        }

        DWORD w = WaitForMultipleObjects(2, handles, FALSE, wait_ms);

        if (w == WAIT_FAILED) {
            result = -1;
            break;
        }
        if (w == WAIT_TIMEOUT) {
            result = 0;
            break;
        }
        /* otherwise loop and re-check; the console handle also fires on
           mouse/focus/resize events, which term_input_available() filters out */
    }

    /* restore blocking mode so recvfrom() behaves as before */
    WSAEventSelect(sockfd, NULL, 0);
    u_long blocking = 0;
    ioctlsocket(sockfd, FIONBIO, &blocking);
    WSACloseEvent(ev);

    return result;
}