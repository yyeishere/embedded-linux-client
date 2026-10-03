#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <poll.h>

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

int main(void){
    
    //printf("client1 is working\n");
    //int abc = 1;
    
    //return 0;

    
       int ports[3] = { 4001, 4002, 4003 };
    struct pollfd sockets[3];

    for (int i = 0; i < 3; i++) {
        int socketno = connect_to_port(ports[i]);
        if (socketno < 0) {
            return 1;
        }
        sockets[i].fd = socketno;
        sockets[i].events = POLLIN;
    }

    for (;;) {
        int ready = poll(sockets, 3, -1);
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
                    printf("port %d: %s", ports[i], buffer);
                    fflush(stdout);
                }
            }
        }
    }

    for (int i = 0; i < 3; i++) {
        close(sockets[i].fd);
    }
    return 0;
}