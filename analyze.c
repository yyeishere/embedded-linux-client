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


long long time_calculator(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
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

    char acc[3][512];
    int acc_len[3];
    static double samples[3][MAX_SAMPLES];
    int sample_count[3];
    for (int i = 0; i < 3; i++) {
        acc_len[i] = 0;
        sample_count[i] = 0;
    }

    long long start = time_calculator();

    while (time_calculator() - start < CAPTURE_SECONDS * 1000) {
        int ready = poll(sockets, 3, 100);
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

                    char *line_start = acc[i];
                    char *nl;
                    while ((nl = strchr(line_start, '\n')) != NULL) {
                        *nl = '\0';
                        if (sample_count[i] < MAX_SAMPLES) {
                            samples[i][sample_count[i]] = atof(line_start);
                            sample_count[i]++;
                        }
                        line_start = nl + 1;
                    }

                    int leftover_len = acc_len[i] - (int)(line_start - acc[i]);
                    memmove(acc[i], line_start, leftover_len);
                    acc_len[i] = leftover_len;
                    acc[i][acc_len[i]] = '\0';
                }
            }
        }
    }

    for (int i = 0; i < 3; i++) {
        close(sockets[i].fd);
    }

    for (int i = 0; i < 3; i++) {
        if (sample_count[i] == 0) {
            printf("out%d: no samples\n", i + 1);
            continue;
        }

        double min = samples[i][0];
        double max = samples[i][0];
        for (int j = 1; j < sample_count[i]; j++) {
            if (samples[i][j] < min) min = samples[i][j];
            if (samples[i][j] > max) max = samples[i][j];
        }
        double amplitude = (max > -min) ? max : -min;

        double mid = (min + max) / 2;
        int crossings = 0;
        for (int j = 1; j < sample_count[i]; j++) {
            if ((samples[i][j - 1] < mid && samples[i][j] >= mid) ||
                (samples[i][j - 1] >= mid && samples[i][j] < mid)) {
                crossings++;
            }
        }
        double frequency = (double)crossings / 2 / CAPTURE_SECONDS;

        double low = min + (max - min) / 3;
        double high = max - (max - min) / 3;
        int middle = 0;
        for (int j = 0; j < sample_count[i]; j++) {
            if (samples[i][j] > low && samples[i][j] < high) middle++;
        }
        double middle_fraction = (double)middle / sample_count[i];
        char *shape;
        if (middle_fraction < 0.10) {
            shape = "square";
        } else if (middle_fraction < 0.28) {
            shape = "sine";
        } else {
            shape = "triangle";
        }

        printf("out%d: samples=%d  min=%.2f  max=%.2f  amplitude=%.2f  rate=%.1f/s  frequency=%.2fHz  mid_frac=%.2f  shape=%s\n",
               i + 1, sample_count[i], min, max, amplitude,
               sample_count[i] / (double)CAPTURE_SECONDS,
               frequency, middle_fraction, shape);
    }

    return 0;
}