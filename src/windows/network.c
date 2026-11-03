#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include "../network.h"

#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#endif

#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif

char ip[INET_ADDRSTRLEN];

/* perror() equivalent for Winsock errors */
static void sock_perror(const char *msg) {
    int err = WSAGetLastError();
    char *text = NULL;

    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, (DWORD)err, 0, (LPSTR)&text, 0, NULL);

    if (text) {
        size_t n = strlen(text);
        while (n > 0 && (text[n - 1] == '\r' || text[n - 1] == '\n'))
            text[--n] = '\0';
        fprintf(stderr, "%s: %s\n", msg, text);
        LocalFree(text);
    } else {
        fprintf(stderr, "%s: error %d\n", msg, err);
    }
}

int get_local_ip(char *buffer, size_t size) {
    ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
                  GAA_FLAG_SKIP_DNS_SERVER;
    ULONG len = 15000;
    IP_ADAPTER_ADDRESSES *adapters = NULL;
    ULONG rc;
    int tries = 0;

    do {
        free(adapters);
        adapters = (IP_ADAPTER_ADDRESSES *)malloc(len);
        if (adapters == NULL)
            return -1;
        rc = GetAdaptersAddresses(AF_INET, flags, NULL, adapters, &len);
    } while (rc == ERROR_BUFFER_OVERFLOW && ++tries < 3);

    if (rc != NO_ERROR) {
        free(adapters);
        return -1;
    }

    int result = -1;

    for (IP_ADAPTER_ADDRESSES *a = adapters; a != NULL && result != 0; a = a->Next) {
        if (a->IfType == IF_TYPE_SOFTWARE_LOOPBACK ||
            a->OperStatus != IfOperStatusUp)
            continue;

        for (IP_ADAPTER_UNICAST_ADDRESS *ua = a->FirstUnicastAddress;
             ua != NULL; ua = ua->Next) {
            if (ua->Address.lpSockaddr == NULL ||
                ua->Address.lpSockaddr->sa_family != AF_INET)
                continue;

            struct sockaddr_in *addr = (struct sockaddr_in *)ua->Address.lpSockaddr;

            if (inet_ntop(AF_INET, &addr->sin_addr, buffer, size) != NULL) {
                result = 0;
                break;
            }
        }
    }

    free(adapters);
    return result;
}

int net_init() {
    WSADATA wsa;

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return -1;

    return get_local_ip(ip, sizeof(ip));
}

void socket_bind(socket_t sockfd) {
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(5000);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
        sock_perror("bind");
        closesocket(sockfd);
        exit(EXIT_FAILURE);
    }
}

socket_t net_socket(void) {
    socket_t sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (sockfd == INVALID_SOCKET) {
        sock_perror("socket");
        exit(EXIT_FAILURE);
    }

    /* Windows reports ICMP "port unreachable" from a previous sendto() as a
       recvfrom() error (WSAECONNRESET). Linux doesn't for unconnected UDP
       sockets, so turn it off to keep behavior identical. */
    BOOL behavior = FALSE;
    DWORD bytes = 0;
    WSAIoctl(sockfd, SIO_UDP_CONNRESET, &behavior, sizeof(behavior),
             NULL, 0, &bytes, NULL, NULL);

    socket_bind(sockfd);

    return sockfd;
}

ssize_t udp_recv(socket_t sockfd, char *buffer, size_t size) {
    int len = (size > INT_MAX) ? INT_MAX : (int)size;
    int length = recvfrom(sockfd, buffer, len, 0, NULL, NULL);

    if (length == SOCKET_ERROR) {
        /* Linux silently truncates oversized datagrams; Windows errors out. */
        if (WSAGetLastError() == WSAEMSGSIZE)
            return (ssize_t)len;

        sock_perror("recvfrom");
        closesocket(sockfd);
        exit(EXIT_FAILURE);
    }

    return (ssize_t)length;
}

ssize_t udp_send(socket_t sockfd, const char *dest_ip, const char *buffer, size_t size) {
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(5000);

    if (inet_pton(AF_INET, dest_ip, &addr.sin_addr) != 1) {
        fprintf(stderr, "Invalid destination IP: %s\n", dest_ip);
        return -1;
    }

    int len = (size > INT_MAX) ? INT_MAX : (int)size;
    int length = sendto(sockfd, buffer, len, 0,
                        (struct sockaddr *)&addr, sizeof(addr));

    if (length == SOCKET_ERROR) {
        sock_perror("sendto");
        return -1;
    }

    return (ssize_t)length;
}

void net_cleanup(socket_t sockfd) {
    closesocket(sockfd);
    WSACleanup();
}