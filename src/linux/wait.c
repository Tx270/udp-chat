#include "../wait.h"
#include <poll.h>
#include <unistd.h>
#include <errno.h>

int poll_wait(socket_t sockfd, int timeout_ms) {
    struct pollfd fds[2] = {
        {
            .fd = STDIN_FILENO,
            .events = POLLIN
        },
        {
            .fd = sockfd,
            .events = POLLIN
        }
    };

    int result;

    do {
        result = poll(fds, 2, timeout_ms);
    } while (result < 0 && errno == EINTR);

    if (result < 0)
        return -1;

    if (result == 0)
        return 0;

    int ready = 0;

    if (fds[0].revents & POLLIN)
        ready |= POLL_INPUT;

    if (fds[1].revents & POLLIN)
        ready |= POLL_SOCKET;

    return ready;
}