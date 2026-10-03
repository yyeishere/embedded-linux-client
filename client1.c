#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <poll.h>
#include <time.h>

#include "clients.h"


int connect_to_port(int port) {
    int socketno = socket(AF_INET, SOCK_STREAM, 0);
    if (socketno < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &server.sin_addr);

    if (connect(socketno, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("connect");
        close(socketno);
        return -1;
    }

    return socketno;
}


long long time_calculator(void) {  //long long to prevent overflow might change the algorithm later
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}


long long timestamp_calculator(void) { // bu da long long olacak
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}



int main(void) {
    int ports[NUM_PORTS] = { PORT_1, PORT_2, PORT_3 };
    struct pollfd sockets[3];

    for (int i = 0; i < 3; i++) {
        int socketno = connect_to_port(ports[i]);
        if (socketno < 0) {
            return 1;
        }
        sockets[i].fd = socketno;
        sockets[i].events = POLLIN;
    }

    char last_values[3][64];
    for (int i = 0; i < 3; i++) {
        strcpy(last_values[i], "--");
    }
    
    long long next_deadline = time_calculator() + CLIENT1_WINDOW_MS;

    for (;;) {
        long long now = time_calculator();
        int timeout = (int)(next_deadline - now);
        if (timeout < 0) {
            timeout = 0;
        }

        int ready = poll(sockets, 3, timeout);
        if (ready < 0) {
            perror("poll");
            break;
        }

        for (int i = 0; i < 3; i++) {
            if (sockets[i].revents & POLLIN) {
                char buffer[256];
                ssize_t n = read(sockets[i].fd, buffer, sizeof(buffer) - 1);

                if (n > 0) {
                    buffer[n] = '\0';
                    size_t len = strlen(buffer);
                    while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r')) {
                        buffer[--len] = '\0';
                    }

                    char *last_line = strrchr(buffer, '\n');
                    char *value = last_line ? last_line + 1 : buffer;

                    strncpy(last_values[i], value, sizeof(last_values[i]) - 1);
                    last_values[i][sizeof(last_values[i]) - 1] = '\0';
                }
            }

        }

        if (time_calculator() >= next_deadline) {
            printf("{\"timestamp\": %lld, \"out1\": \"%s\", \"out2\": \"%s\", \"out3\": \"%s\"}\n",timestamp_calculator(), last_values[0], last_values[1], last_values[2]);
            fflush(stdout);

            for (int i = 0; i < 3; i++) {
                strcpy(last_values[i], "--");
            }
            next_deadline += CLIENT1_WINDOW_MS;
        }
    }

    for (int i = 0; i < 3; i++) {
        close(sockets[i].fd);
    }
    return 0;
}