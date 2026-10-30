#include "network.h"
#include "terminal.h"
#include "display.h"
#include "wait.h"
#include <stdio.h>
#include <string.h>


char char_buffer[16] = {0};
char message_buffer[300] = {0};


void send_message(socket_t sockfd) {
    int length = snprintf(
        message_buffer,
        sizeof(message_buffer),
        "\033[1m\033[34m%s\033[0m: %s",
        ip,
        input_buffer
    );

    if (length < 0 || (size_t)length >= sizeof(message_buffer)) return;

    if (udp_send(sockfd, "127.0.0.1", message_buffer, (size_t)length) < 0) return;
    
    // print_message(message_buffer);
    input_buffer[0] = '\0';
}


void handle_input(socket_t sockfd) {
    ssize_t length = get_input(char_buffer, sizeof(char_buffer));
    if (length <= 0 || char_buffer[0] == 0x1B) return;

    for (int i = 0; i < length; i++) {
        if (char_buffer[i] >= 0x20 && char_buffer[i] <= 0x7E) {
            size_t len = strlen(input_buffer);

            if (len < sizeof(input_buffer) - 1) {
                input_buffer[len] = char_buffer[i];
                input_buffer[len + 1] = '\0';
            }
        } 
        else if (char_buffer[i] == 0x08 || char_buffer[i] == 0x7F) {
            size_t len = strlen(input_buffer);

            if (len > 0) {
                input_buffer[len - 1] = '\0';
            }
        }
        else if (char_buffer[i] == 0x0A && strlen(input_buffer) != 0) {
            send_message(sockfd);
        }
        else if (char_buffer[i] == 0x04) {
            running = 0;
            break;
        }
    }

    display();
}


void handle_socket(socket_t sockfd) {
    ssize_t length = udp_recv(sockfd, message_buffer, sizeof(message_buffer) - 1);

    if (length <= 0) return;

    message_buffer[length] = '\0';
    print_message(message_buffer);
}


int main(void) {
    if (term_init() == -1) {
        perror("term_init");
        return -1;
    }

    if (net_init() == -1) {
        term_cleanup();
        return -1;
    }

    socket_t sockfd = net_socket();

    display();
    
    while (running) {
        int ready = poll_wait(sockfd, 250);

        if (ready < 0) {
            perror("poll_wait");
            break;
        }

        if (ready & POLL_INPUT) {
            handle_input(sockfd);
        }

        if (ready & POLL_SOCKET) {
            handle_socket(sockfd);
        }
    }

    net_cleanup(sockfd);
    term_cleanup();
    return 0;
}