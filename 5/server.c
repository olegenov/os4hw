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

    return 0;
}

sig_atomic_t killed = 0;
int master_socket;

void killing_handler(int sig) {
    killed = 1;
    close(master_socket);
}

void terminate_players(int sig) {
    kill(0, SIGTERM);
}

int main(int argc, char** argv) {
    srand(time(NULL));

    signal(SIGINT, killing_handler);

    if (argc != 2 && argc != 4) {
        perror("incorrect argument count! it must be 1 (<N>) or 3 (<ip> <port> <N>)");
        exit(EXIT_FAILURE);
    }

    int N;
    sscanf(argv[argc - 1], "%d", &N);

    master_socket = socket(AF_INET, SOCK_DGRAM, 0);
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
        int port = (argc == 4) ? atoi(argv[2]) : PORT;
        printf("Server binded to port %d successfully.\n", port);
    }

    struct sockaddr_in client_addr[N];
    socklen_t client_len;

    printf("Waiting for players to connect\n");
    char buf[BUFFER_SIZE];

    for (int i = 0; i < N; ++i) {
        memset(&client_addr[i], 0, sizeof(client_addr[i]));
        client_len = sizeof(client_addr[i]);
        recvfrom(master_socket, buf, BUFFER_SIZE, MSG_WAITALL, (struct sockaddr *) &client_addr[i], &client_len);
        int id;
        sscanf(buf, "%d", &id);
        printf("Player %d connected\n", i);
    }

    int *tournament = mmap(NULL, N * N * sizeof(int),
                           PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    if (tournament == MAP_FAILED) {
        perror("mmap failed");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            tournament[i * N + j] = -1;
        }
    }

    for (int i = 0; i < N; ++i) {
        sprintf(buf, "%d %d", N, i);
        sendto(master_socket, buf, BUFFER_SIZE, MSG_CONFIRM, (struct sockaddr*)&client_addr[i], client_len);
    }

    int rounds = 0;
    for (int i = 0; i < N; ++i) {
        if (fork() == 0) {
            srand(time(NULL) ^ getpid());

            while (!killed && rounds < N * (N - 1) / 2) {
                int j;
                recvfrom(master_socket, buf, BUFFER_SIZE, MSG_WAITALL, (struct sockaddr*)&client_addr[i], &client_len);
                sscanf(buf, "%d", &j);

                if (tournament[i * N + j] != -1 || i == j) continue;

                int i_result = rand() % 3;
                int j_result = rand() % 3;

                int result = battle(i_result, j_result);
                tournament[i * N + j] = result;
                tournament[j * N + i] = 2 - result;

                sprintf(buf, "Player %d (%s) vs Player %d (%s) => Result: %s",
                        i + 1, sign_str[i_result], j + 1, sign_str[j_result],
                        result == 0 ? "Player 1 wins\n" : (result == 2 ? "Player 2 wins\n" : "Draw\n"));

                sendto(master_socket, buf, BUFFER_SIZE, MSG_CONFIRM, (struct sockaddr*)&client_addr[i], client_len);
                sendto(master_socket, buf, BUFFER_SIZE, MSG_CONFIRM, (struct sockaddr*)&client_addr[j], client_len);

                printf("Player id %d played with id %d: winner: %s", i+1, j+1, result == 0 ? "Player 1 wins\n" : (result == 2 ? "Player 2 wins\n" : "Draw\n"));

                rounds++;
            }
            exit(0);
        }
    }

    for (int i = 0; i < N; ++i) {
        wait(NULL);
    }

    munmap(tournament, N * N * sizeof(int));
    close(master_socket);
    return 0;
}
