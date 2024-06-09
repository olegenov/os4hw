#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>

#define PORT 8888
#define BUFFER_SIZE 2048
#define MSG_CONFIRM 0x800

sig_atomic_t killed = 0;
void killing_handler(int sig) {
    killed = 1;
}

int main(int argc, char** argv) {
    ssize_t nread;
    signal(SIGINT, killing_handler);
    char buf[BUFFER_SIZE];

    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd == -1) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    } else {
        printf("Viewer socket created.\n");
    }

    struct sockaddr_in addr = { 0 };
    socklen_t addrlen = sizeof(addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    if (argc == 2) {
        addr.sin_addr.s_addr = inet_addr(argv[0]);
        int port;
        sscanf(argv[1], "%d", &port);
        addr.sin_port = htons(port);
    }

    // Connecting to server
    sendto(socket_fd, "1", BUFFER_SIZE, MSG_CONFIRM, (const struct sockaddr *) &addr, addrlen);
    while (1) {
        if (killed) {
            close(socket_fd);
            exit(0);
        }

        recvfrom(socket_fd, buf, BUFFER_SIZE, MSG_WAITALL, (struct sockaddr*)&addr, &addrlen);
        printf("%s\n", buf);
    }

    return 0;
}
