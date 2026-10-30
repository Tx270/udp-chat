#include "network.h"
#include "terminal.h"
#include "display.h"
#include "wait.h"
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>


int main(void) {
    term_init();
    net_init();
    socket_t sockfd = net_socket();

    char input_buffer[128] = {0};
    char char_buffer[8] = {0};
    char message_buffer[128] = {0};

    display();

    while (1) {
        int ready = poll_wait(sockfd, -1);

        if (ready < 0) break;

        if (ready & NET_INPUT) {
            ssize_t length = get_input(char_buffer, sizeof(char_buffer));
            if (length <= 0 || char_buffer[0] == 0x1B) continue;

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
                    snprintf(
                        message_buffer,
                        sizeof(message_buffer),
                        "\033[1m\033[34m%s\033[0m: %s",
                        ip,
                        input_buffer
                    );
                    
                    udp_send(sockfd, "127.0.0.1", message_buffer, strlen(message_buffer));
                    
                    input_buffer[0] = '\0';
                }
            }

            print_input(input_buffer);
        }

        if (ready & NET_SOCKET) {
            ssize_t length = udp_recv(sockfd, message_buffer, sizeof(message_buffer));

            if (length <= 0) continue;

            print_message(message_buffer);
        }
    }

    net_cleanup(sockfd);
    term_cleanup();
    return 0;
}