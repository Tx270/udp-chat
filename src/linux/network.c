#include "../network.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <threads.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>


char ip[INET_ADDRSTRLEN];

int get_local_ip(char *buffer, size_t size) {
    struct ifaddrs *interfaces;

    if (getifaddrs(&interfaces) == -1)
        return -1;

    int result = -1;

    for (struct ifaddrs *ifa = interfaces; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL ||
            ifa->ifa_addr->sa_family != AF_INET ||
            (ifa->ifa_flags & IFF_LOOPBACK))
            continue;

        struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;

        if (inet_ntop(AF_INET, &addr->sin_addr, buffer, size) != NULL) {
            result = 0;
            break;
        }
    }

    freeifaddrs(interfaces);
    return result;
}

int net_init() {
    return get_local_ip(ip, sizeof(ip));
}

void socket_bind(socket_t sockfd) {
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(5000),
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
}

socket_t net_socket(void) {
    socket_t sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    socket_bind(sockfd);

    return sockfd;
}

ssize_t udp_recv(socket_t sockfd, char *buffer, size_t size) {
    ssize_t length = recvfrom(sockfd, buffer, size, 0, NULL, NULL);

    if (length < 0) {
        perror("recvfrom");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    return length;
}

ssize_t udp_send(socket_t sockfd, const char *dest_ip, const char *buffer, size_t size) {
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(5000)
    };

    if (inet_pton(AF_INET, dest_ip, &addr.sin_addr) != 1) {
        fprintf(stderr, "Invalid destination IP: %s\n", dest_ip);
        return -1;
    }

    ssize_t length = sendto(
        sockfd,
        buffer,
        size,
        0,
        (struct sockaddr *)&addr,
        sizeof(addr)
    );

    if (length < 0) {
        perror("sendto");
        return -1;
    }

    return length;
}

void net_cleanup(socket_t sockfd) {
    close(sockfd);
}