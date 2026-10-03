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

    char acc[3][512];
    int acc_len[3];
    for (int i = 0; i < 3; i++) {
        acc_len[i] = 0;
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
                ssize_t n = read(sockets[i].fd, acc[i] + acc_len[i], sizeof(acc[i]) - acc_len[i] - 1);

                if (n > 0) {
                    acc_len[i] += n;
                    acc[i][acc_len[i]] = '\0';

                    char *last_nl = strrchr(acc[i], '\n');
                    if (last_nl != NULL) {
                        *last_nl = '\0';
                        char *prev_nl = strrchr(acc[i], '\n');
                        char *value = prev_nl ? prev_nl + 1 : acc[i];

                        size_t vlen = strlen(value);
                        if (vlen > 0 && value[vlen - 1] == '\r') {
                            value[vlen - 1] = '\0';
                        }

                        strncpy(last_values[i], value, sizeof(last_values[i]) - 1);
                        last_values[i][sizeof(last_values[i]) - 1] = '\0';

                        char *leftover = last_nl + 1;
                        int leftover_len = acc_len[i] - (int)(leftover - acc[i]);
                        memmove(acc[i], leftover, leftover_len);
                        acc_len[i] = leftover_len;
                        acc[i][acc_len[i]] = '\0';
                    }
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