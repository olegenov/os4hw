#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

#define PORT 8888
#define BUFFER_SIZE 2048
#define MSG_CONFIRM 0x800

sig_atomic_t killed = 0;
void killing_handler(int sig) {
    killed = 1;
}

int main(int argc, char** argv) {
    signal(SIGINT, killing_handler);
    char buf[BUFFER_SIZE];

    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd == -1) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    else {
        printf("Player socket created\n");
    }

    struct sockaddr_in addr = { 0 };
    socklen_t addrlen = sizeof addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);

    if (argc == 2) {
        addr.sin_addr.s_addr = inet_addr(argv[0]);
        int port;
        sscanf(argv[1], "%d", &port);
        addr.sin_port = htons(port);
    }

    // Sends connect request
    sendto(socket_fd, "1", BUFFER_SIZE, MSG_CONFIRM, (const struct sockaddr *) &addr, addrlen);

    // Receives N and id from server
    int N;
    int id;

    recvfrom(socket_fd, buf, BUFFER_SIZE, MSG_WAITALL, (struct sockaddr *) &addr, &addrlen);
    sscanf(buf, "%d %d", &N, &id);
    printf("Player id: %d\n", id);

    int players[N];
    for (int i = 0; i < N; ++i) {
        players[i] = i == id;
    }

    int rounds = 0;
    for (int i = 0; i < 2; ++i) {
        if (fork() != 0) {
            continue;
        }

        while (rounds < N - 1) {
            if (killed) {
                close(socket_fd);
                exit(0);
            }

            if (i == 0) {
                // One thread is choosing an opponent and sending battle request
                for (int i = 0; i < N; ++i) {
                    if (!players[i]) {
                        sprintf(buf, "%d", i);
                        sendto(socket_fd, buf, BUFFER_SIZE, MSG_CONFIRM, (struct sockaddr*)&addr, addrlen);
                        sleep(3);
                    }
                }
            } else {
                // Other thread is receiving who player battled with
                recvfrom(socket_fd, buf, BUFFER_SIZE, MSG_WAITALL, (struct sockaddr *) &addr, &addrlen);
                sscanf(buf, "%d", &id);
                players[id] = 1;
                ++rounds;
            }
        }

        exit(0);
    }

    return 0;
}
