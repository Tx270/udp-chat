#include "network.h"
#define POLL_INPUT  1
#define POLL_SOCKET 2

int poll_wait(socket_t sockfd, int timeout_ms);