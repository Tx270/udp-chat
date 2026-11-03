#pragma once
#define _DEFAULT_SOURCE
#include <netinet/in.h>
#include <stddef.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef SOCKET socket_t;
    #ifdef _MSC_VER
    #include <BaseTsd.h>
    typedef SSIZE_T ssize_t;
    #endif
#else
    #include <sys/types.h>
    typedef int socket_t;
    typedef ssize_t net_length_t;
#endif

extern char ip[INET_ADDRSTRLEN];

socket_t net_socket(void);
ssize_t udp_recv(socket_t sockfd, char *buffer, size_t size);
ssize_t udp_send(socket_t sockfd, const char *dest_ip, const char *buffer, size_t size);
int net_init();
void net_cleanup(socket_t sockfd);