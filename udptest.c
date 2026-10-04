#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdint.h>
#include <arpa/inet.h>



int main(int argc, char *argv[]) {

    int sock = socket(AF_INET, SOCK_DGRAM, 0);


    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(4000);
    inet_pton(AF_INET, "127.0.0.1", &server.sin_addr);

    uint16_t msg[4];
    int nfields = argc - 1;

    for (int i = 0; i < nfields; i++) {
        msg[i] = htons((uint16_t)atoi(argv[i + 1]));
    }

    size_t len = nfields * sizeof(uint16_t);

    ssize_t sent = sendto(sock, msg, len, 0,
                          (struct sockaddr *)&server, sizeof(server));
    if (sent < 0) {
        perror("sendto");
        close(sock);
        return 1;
    }

    printf("sent %zd bytes (%d fields) to 127.0.0.1:4000\n", sent, nfields);

    return 0;
}