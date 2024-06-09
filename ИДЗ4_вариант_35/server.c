#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <time.h>

#define PORT 8888
#define BUFFER_SIZE 2048
#define MSG_CONFIRM 0x800

enum sign {
    ROCK,
    PAPER,
    SCISSORS
};

const char* const sign_str[] = {
        [ROCK] = "rock",
        [PAPER] = "paper",
        [SCISSORS] = "scissors",
};

int battle(int a, int b) {
    if (a == b)
        return 1;

    if (a == ROCK) {
        if (b == PAPER)
            return 2;
        return 0;
    } else if (a == PAPER) {
        if (b == SCISSORS)
            return 2;
        return 0;
    } else if (a == SCISSORS) {
        if (b == ROCK)
            return 2;
        return 0;
    }
}

sig_atomic_t killed = 0;
void killing_handler(int sig) {
    killed = 1;
}

void terminate_players(int sig) {
    kill(0, SIGTERM);
}

int main(int argc, char** argv) {
    srand(time(NULL));

    char buf[BUFFER_SIZE];
    signal(SIGINT, killing_handler);
    signal(SIGINT, terminate_players);

    if (argc != 2 && argc != 4) {
        perror("incorrect argument count! it must be 1 (<N>) or 3 (<ip> <port> <N>)");
        exit(EXIT_FAILURE);
    }

    int N;
    sscanf(argv[argc - 1], "%d", &N);

    int master_socket = socket(AF_INET, SOCK_DGRAM, 0);

    if (master_socket == -1) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    } else {
        printf("Server socket created.\n");
    }

    struct sockaddr_in addr = { 0 };
    socklen_t addrlen = sizeof(addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);

    if (argc == 4) {
        addr.sin_addr.s_addr = inet_addr(argv[1]);
        int port;
        sscanf(argv[2], "%d", &port);
        addr.sin_port = htons(port);
    }

    int bind_res = bind(master_socket, (struct sockaddr *) & addr, addrlen);

    if (bind_res == -1) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    } else {
        int port = PORT;

        if (argc == 4) {
            port = argv[2];
        }

        printf("Server binded to port %d successfully.\n", port);
    }

    struct sockaddr_in client_addr[N + 1];
    socklen_t client_len;

    printf("Waiting for viewer and players to connect\n");
    printf("First, connect all players, then the viewer\n");

    for (int i = 0; i < N + 1; ++i) {
        memset(&client_addr[i], 0, sizeof(client_addr[i]));
        // Request to connect
        client_len = sizeof(client_addr[i]);
        recvfrom(master_socket, buf, BUFFER_SIZE, MSG_WAITALL, (struct sockaddr *) &client_addr[i], &client_len);

        if (i == N) {
            printf("Viewer connected\n");
        } else {
            printf("Player %d connected\n", i);
        }
    }

    int *tournament;
    tournament = (int*) mmap(NULL, N * N * sizeof(int),
                             PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED,
                             -1, 0);
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            tournament[i * N + j] = -1;
        }
    }

    sprintf(buf, "Tournament begins!\n");
    sendto(master_socket, buf, BUFFER_SIZE, MSG_CONFIRM, (const struct sockaddr*)&(client_addr[N]), client_len);

    for (int i = 0; i < N; ++i) {
        // Sends N and id after everyone connected
        sprintf(buf, "%d %d", N, i);
        sendto(master_socket, buf, BUFFER_SIZE, MSG_CONFIRM, (const struct sockaddr*)&(client_addr[i]), client_len);
    }

    int rounds = 0;
    for (int i = 0; i < N; ++i) {
        if (fork() != 0) {
            continue;
        }

        srand(time(NULL) ^ getpid());

        while (!killed && rounds < N * (N - 1) / 2) {
            // Receiving a battle request
            int j;

            recvfrom(master_socket, buf, BUFFER_SIZE, MSG_WAITALL, (struct sockaddr *) &client_addr[i], &client_len);
            sscanf(buf, "%d", &j);

            if (tournament[i * N + j] != -1 || i == j) {
                continue;
            }

            // Sending to viewer
            sprintf(buf, "Player #%i plays with player #%i", i + 1, j + 1);
            printf("Player #%i plays with player #%i", i + 1, j + 1);
            sendto(master_socket, buf, BUFFER_SIZE, MSG_CONFIRM, (const struct sockaddr*)&(client_addr[N]), client_len);

            int r = rand();

            int i_result = rand() % 3;
            int j_result = rand() % 3;

            int result = battle(i_result, j_result);
            tournament[i * N + j] = result;
            tournament[j * N + i] = 2 - result;

            // Sending to viewer
            sprintf(buf, "Player #%i: %s. Player #%i: %s.    %i:%i",
                    i + 1, sign_str[result], j + 1, sign_str[2 - result], result, 2 - result);
            sendto(master_socket, buf, BUFFER_SIZE, MSG_CONFIRM, (const struct sockaddr*)&(client_addr[N]), client_len);

            // Sending table to viewer
            strncpy (buf, "Tournament: \n", BUFFER_SIZE);
            char num[126];
            for (int i = 0; i < N; ++i) {
                for (int j = 0; j < N; ++j) {
                    sprintf(num, "%d ", tournament[i * N + j]);
                    strncat (buf, num, BUFFER_SIZE);
                }
                sprintf(num, "\n");
                strcat(buf, num);
            }
            sendto(master_socket, buf, BUFFER_SIZE, MSG_CONFIRM, (const struct sockaddr*)&(client_addr[N]), client_len);

            // Sending opponent info
            sprintf(buf, "%d", j);
            sendto(master_socket, buf, BUFFER_SIZE, MSG_CONFIRM, (const struct sockaddr*)&(client_addr[i]), client_len);

            sprintf(buf, "%d", i);
            sendto(master_socket, buf, BUFFER_SIZE, MSG_CONFIRM, (const struct sockaddr*)&(client_addr[j]), client_len);

            ++rounds;
        }

        exit(0);
    }

    return 0;
}
