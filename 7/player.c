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
int socket_fd;

void killing_handler(int sig) {
    killed = 1;
    close(socket_fd);
    exit(0);
}

int main(int argc, char** argv) {
    signal(SIGINT, killing_handler);
    char buf[BUFFER_SIZE];

    socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd == -1) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    } else {
        printf("Player socket created\n");
    }

    struct sockaddr_in addr = { 0 };
    socklen_t addrlen = sizeof(addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);

    if (argc == 3) {
        addr.sin_addr.s_addr = inet_addr(argv[1]);
        int port;
        sscanf(argv[2], "%d", &port);
        addr.sin_port = htons(port);
    } else {
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    }

    sendto(socket_fd, "1", BUFFER_SIZE, MSG_CONFIRM, (const struct sockaddr *) &addr, addrlen);

    int N;
    int id;

    recvfrom(socket_fd, buf, BUFFER_SIZE, MSG_WAITALL, (struct sockaddr *) &addr, &addrlen);
    sscanf(buf, "%d %d", &N, &id);
    printf("Player id: %d\n", id);

    int players[N];
    for (int i = 0; i < N; ++i) {
        players[i] = (i == id);
    }

    int rounds = 0;

    pid_t sender_pid = fork();
    if (sender_pid == 0) {
        while (rounds < N - 1) {
            if (killed) {
                close(socket_fd);
                exit(0);
            }

            for (int j = 0; j < N; ++j) {
                if (!players[j] && j != id) {
                    sprintf(buf, "%d", j);
                    printf("Sends request to %d\n", j);
                    sendto(socket_fd, buf, BUFFER_SIZE, MSG_CONFIRM, (struct sockaddr *) &addr, addrlen);
                    sleep(1);
                }
            }
            ++rounds;
        }
        exit(0);
    }

    pid_t receiver_pid = fork();
    if (receiver_pid == 0) {
        while (rounds < N - 1) {
            if (killed) {
                close(socket_fd);
                exit(0);
            }

            recvfrom(socket_fd, buf, BUFFER_SIZE, MSG_WAITALL, (struct sockaddr *) &addr, &addrlen);
            int opponent_id;
            if (rounds >= N - 1) {
                exit(0);
            }
            sscanf(buf, "%d", &opponent_id);
            printf("Gets request from %d\n", opponent_id);
            printf("%s", buf);
            players[opponent_id] = 1;
            ++rounds;
        }
        exit(0);
    }

    waitpid(sender_pid, NULL, 0);
    waitpid(receiver_pid, NULL, 0);

    close(socket_fd);

    return 0;
}
