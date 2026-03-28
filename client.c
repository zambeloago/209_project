#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>

#include "common.h"
#include "protocol.h"
#include "utils.h"

#define SERVER_ADDR "127.0.0.1"

static int my_turn = 0; /* 1 when it is our turn to enter a move */

/* Send a newline-terminated message to the server. */
static void send_msg(int fd, const char *msg) {
    char buf[BUFFER_SIZE];
    snprintf(buf, sizeof(buf), "%s\n", msg);
    if (send(fd, buf, strlen(buf), 0) < 0) {
        perror("send");
        exit(EXIT_FAILURE);
    }
}

/* Handle a single message received from the server. */
static int handle_server_msg(const char *msg) {
    /* Make a mutable copy for trim_newline */
    char buf[BUFFER_SIZE];
    strncpy(buf, msg, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    trim_newline(buf);

    if (strcmp(buf, MSG_WAIT) == 0) {
        printf("Waiting for another player...\n");
        fflush(stdout);

    } else if (strcmp(buf, MSG_START) == 0) {
        printf("Game started!\n");
        fflush(stdout);

    } else if (strcmp(buf, MSG_TURN) == 0) {
        my_turn = 1;
        printf("Your turn! Enter row and column (e.g. 2 3): ");
        fflush(stdout);

    } else if (strcmp(buf, MSG_HIT) == 0) {
        printf("*** HIT! ***\n");
        fflush(stdout);

    } else if (strcmp(buf, MSG_MISS) == 0) {
        printf("*** MISS. ***\n");
        my_turn = 0;
        fflush(stdout);

    } else if (strcmp(buf, MSG_INVALID) == 0) {
        printf("Invalid move. Try again: ");
        fflush(stdout);

    } else if (strcmp(buf, MSG_WIN) == 0) {
        printf("*** You WIN! Congratulations! ***\n");
        fflush(stdout);
        return 0; /* signal to exit */

    } else if (strcmp(buf, MSG_LOSE) == 0) {
        printf("*** You LOSE. Better luck next time. ***\n");
        fflush(stdout);
        return 0; /* signal to exit */

    } else if (strncmp(buf, MSG_MESSAGE, strlen(MSG_MESSAGE)) == 0) {
        /* "MESSAGE <text>" */
        const char *text = buf + strlen(MSG_MESSAGE);
        while (*text == ' ') text++;
        printf("[server] %s\n", text);
        fflush(stdout);

    } else {
        printf("[unknown] %s\n", buf);
        fflush(stdout);
    }

    return 1; /* continue running */
}

int main(int argc, char *argv[]) {
    const char *server_ip = SERVER_ADDR;
    int port = PORT;

    /* Optional: ./client <server_ip> <port> */
    if (argc >= 2) server_ip = argv[1];
    if (argc >= 3) port      = atoi(argv[2]);

    /* ----- Connect to server ----- */
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) { perror("socket"); exit(EXIT_FAILURE); }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid server address: %s\n", server_ip);
        exit(EXIT_FAILURE);
    }

    if (connect(sock_fd, (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    printf("Connected to server at %s:%d\n", server_ip, port);

    /* Optionally send a JOIN with a username */
    char join_msg[BUFFER_SIZE];
    printf("Enter your username: ");
    fflush(stdout);
    if (fgets(join_msg, sizeof(join_msg), stdin) == NULL) {
        close(sock_fd);
        return 0;
    }
    trim_newline(join_msg);

    char join_buf[BUFFER_SIZE];
    snprintf(join_buf, sizeof(join_buf), "%s %s", MSG_JOIN, join_msg);
    send_msg(sock_fd, join_buf);

    /* ----- Main select() loop ----- */
    fd_set read_fds;
    int running = 1;

    while (running) {
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds); /* monitor keyboard */
        FD_SET(sock_fd, &read_fds);       /* monitor server   */

        int max_fd = sock_fd > STDIN_FILENO ? sock_fd : STDIN_FILENO;

        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (activity < 0) {
            if (errno == EINTR) continue;
            perror("select");
            break;
        }

        /* ----- Data from server ----- */
        if (FD_ISSET(sock_fd, &read_fds)) {
            char buf[BUFFER_SIZE];
            memset(buf, 0, sizeof(buf));
            int bytes = recv(sock_fd, buf, sizeof(buf) - 1, 0);
            if (bytes <= 0) {
                printf("Server closed the connection.\n");
                break;
            }
            buf[bytes] = '\0';

            /*
             * The server may send multiple newline-separated messages in one
             * recv() call.  Split on '\n' and handle each one.
             */
            char *line = buf;
            char *nl;
            while ((nl = strchr(line, '\n')) != NULL) {
                *nl = '\0';
                if (strlen(line) > 0) {
                    if (!handle_server_msg(line)) {
                        running = 0;
                        break;
                    }
                }
                line = nl + 1;
            }
            /* Handle any trailing fragment without a newline */
            if (running && strlen(line) > 0)
                if (!handle_server_msg(line))
                    running = 0;
        }

        /* ----- User input from stdin ----- */
        if (running && FD_ISSET(STDIN_FILENO, &read_fds)) {
            char input[BUFFER_SIZE];
            if (fgets(input, sizeof(input), stdin) == NULL) {
                /* EOF on stdin */
                break;
            }
            trim_newline(input);

            if (!my_turn) {
                printf("It's not your turn. Please wait.\n");
                fflush(stdout);
                continue;
            }

            /* User enters "<row> <col>"; we send "MOVE <row> <col>" */
            int row, col;
            if (sscanf(input, "%d %d", &row, &col) == 2) {
                char move_msg[BUFFER_SIZE];
                snprintf(move_msg, sizeof(move_msg), "%s %d %d",
                         MSG_MOVE, row, col);
                send_msg(sock_fd, move_msg);
                my_turn = 0; /* wait for server response */
            } else {
                printf("Invalid input. Enter row and column as two numbers "
                       "(e.g. 2 3): ");
                fflush(stdout);
                my_turn = 1; /* let them try again */
            }
        }
    }

    close(sock_fd);
    printf("Disconnected.\n");
    return 0;
}