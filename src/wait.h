#include "network.h"
#define NET_INPUT  1
#define NET_SOCKET 2

int poll_wait(socket_t sockfd, int timeout_ms);