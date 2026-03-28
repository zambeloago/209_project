/*
 * Battleship client: connects to server, drives local I/O, follows server protocol.
 */

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>

#define GRID_N 10
#define MAX_LINE 512

/* -------------------------------------------------------------------------- */
/* Socket helpers                                                             */
/* -------------------------------------------------------------------------- */

static ssize_t send_all(int fd, const char *buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, buf + sent, len - sent, 0);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (n == 0)
            return -1;
        sent += (size_t)n;
    }
    return (ssize_t)len;
}

static int send_line(int fd, const char *s) {
    size_t len = strlen(s);
    if (send_all(fd, s, len) < 0)
        return -1;
    if (send_all(fd, "\n", 1) < 0)
        return -1;
    return 0;
}

static int recv_line(int fd, char *buf, size_t cap) {
    size_t i = 0;
    while (i + 1 < cap) {
        char c;
        ssize_t n = recv(fd, &c, 1, 0);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (n == 0)
            return -1;
        if (c == '\n')
            break;
        if (c != '\r')
            buf[i++] = c;
    }
    buf[i] = '\0';
    return (int)i;
}

static int connect_to_server(const char *host, uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        fprintf(stderr, "Invalid IPv4 address: %s\n", host);
        close(fd);
        return -1;
    }

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(fd);
        return -1;
    }
    return fd;
}

/* -------------------------------------------------------------------------- */
/* Board display (ASCII)                                                      */
/* -------------------------------------------------------------------------- */

static void print_tracking_legend(void) {
    printf("Tracking board:  X = hit   O = miss   - = unknown\n");
}

static void print_own_legend(void) {
    printf("Your board:  . = water   S = ship   X = hit on ship   O = opponent miss\n");
}

static void print_grid(const char title[], char b[GRID_N][GRID_N]) {
    printf("\n%s\n", title);
    printf("    ");
    for (int c = 0; c < GRID_N; c++)
        printf("%d ", c);
    printf("\n");
    for (int r = 0; r < GRID_N; r++) {
        printf(" %c  ", 'A' + r);
        for (int c = 0; c < GRID_N; c++)
            printf("%c ", b[r][c]);
        printf("\n");
    }
    printf("\n");
}

/* Read GRID_N lines from server into board (each line is GRID_N characters). */
static int recv_board(int fd, char b[GRID_N][GRID_N]) {
    for (int r = 0; r < GRID_N; r++) {
        char line[MAX_LINE];
        if (recv_line(fd, line, sizeof(line)) < 0)
            return -1;
        if ((int)strlen(line) != GRID_N) {
            fprintf(stderr, "Protocol error: bad board row length.\n");
            return -1;
        }
        memcpy(b[r], line, (size_t)GRID_N);
    }
    return 0;
}

static void trim_newline(char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r'))
        s[--n] = '\0';
}

/* -------------------------------------------------------------------------- */
/* Main session loop                                                          */
/* -------------------------------------------------------------------------- */

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server_ip> <port>\n", argv[0]);
        return 1;
    }

    char *end = NULL;
    long port_l = strtol(argv[2], &end, 10);
    if (!argv[2][0] || end == argv[2] || *end != '\0' || port_l < 1 || port_l > 65535) {
        fprintf(stderr, "Invalid port.\n");
        return 1;
    }

    int sock = connect_to_server(argv[1], (uint16_t)port_l);
    if (sock < 0)
        return 1;

    printf("Connected to %s:%s\n", argv[1], argv[2]);
    print_tracking_legend();
    print_own_legend();

    char board_track[GRID_N][GRID_N];
    char board_own[GRID_N][GRID_N];

    for (;;) {
        char msg[MAX_LINE];
        if (recv_line(sock, msg, sizeof(msg)) < 0) {
            fprintf(stderr, "Disconnected from server.\n");
            break;
        }

        if (strncmp(msg, "WELCOME ", 8) == 0) {
            printf("You are player %s\n", msg + 8);
            continue;
        }
        if (strcmp(msg, "WAIT_PEER") == 0) {
            printf("Waiting for a second player to connect...\n");
            continue;
        }
        if (strcmp(msg, "PEER_READY") == 0) {
            printf("Opponent connected. Ship placement will begin.\n");
            continue;
        }
        if (strncmp(msg, "PLACE ", 6) == 0) {
            int idx = -1, len = -1;
            if (sscanf(msg, "PLACE %d %d", &idx, &len) != 2 || idx < 0 || len < 1) {
                fprintf(stderr, "Bad PLACE line from server.\n");
                goto out;
            }
            printf("Place ship %d (length %d). Enter endpoints like B2-B4: ", idx + 1, len);
            fflush(stdout);
            char line[MAX_LINE];
            if (!fgets(line, sizeof(line), stdin)) {
                fprintf(stderr, "Input ended.\n");
                goto out;
            }
            trim_newline(line);
            if (send_line(sock, line) != 0)
                goto out;
            continue;
        }
        if (strcmp(msg, "PLACE_OK") == 0) {
            printf("Ship accepted.\n");
            continue;
        }
        if (strncmp(msg, "ERR ", 4) == 0) {
            printf("Server: %s\n", msg + 4);
            continue;
        }
        if (strcmp(msg, "SETUP_DONE") == 0) {
            printf("Setup complete. Game starting — player 0 shoots first.\n");
            continue;
        }
        if (strcmp(msg, "YOUR_TURN") == 0) {
            printf("Your turn. Enter a cell (e.g. C7): ");
            fflush(stdout);
            char line[MAX_LINE];
            if (!fgets(line, sizeof(line), stdin)) {
                fprintf(stderr, "Input ended.\n");
                goto out;
            }
            trim_newline(line);
            /* Allow optional "SHOT C7" by stripping prefix */
            char *p = line;
            while (*p && isspace((unsigned char)*p))
                p++;
            if (strncasecmp(p, "SHOT", 4) == 0) {
                p += 4;
                while (*p && isspace((unsigned char)*p))
                    p++;
            }
            if (send_line(sock, p) != 0)
                goto out;
            continue;
        }
        if (strcmp(msg, "WAIT_TURN") == 0) {
            printf("Waiting for opponent's move...\n");
            continue;
        }
        if (strcmp(msg, "ERR_BAD_SHOT") == 0) {
            printf("Invalid shot format. Use a cell like C7 (row A–J, column 0–9).\n");
            printf("Your turn. Enter a cell: ");
            fflush(stdout);
            char line[MAX_LINE];
            if (!fgets(line, sizeof(line), stdin)) {
                fprintf(stderr, "Input ended.\n");
                goto out;
            }
            trim_newline(line);
            char *p = line;
            while (*p && isspace((unsigned char)*p))
                p++;
            if (strncasecmp(p, "SHOT", 4) == 0) {
                p += 4;
                while (*p && isspace((unsigned char)*p))
                    p++;
            }
            if (send_line(sock, p) != 0)
                goto out;
            continue;
        }
        if (strcmp(msg, "ERR_ALREADY_GUESSED") == 0) {
            printf("You already fired at that cell.\n");
            printf("Your turn. Enter a cell: ");
            fflush(stdout);
            char line[MAX_LINE];
            if (!fgets(line, sizeof(line), stdin)) {
                fprintf(stderr, "Input ended.\n");
                goto out;
            }
            trim_newline(line);
            char *p = line;
            while (*p && isspace((unsigned char)*p))
                p++;
            if (strncasecmp(p, "SHOT", 4) == 0) {
                p += 4;
                while (*p && isspace((unsigned char)*p))
                    p++;
            }
            if (send_line(sock, p) != 0)
                goto out;
            continue;
        }
        if (strncmp(msg, "RESULT ", 7) == 0) {
            const char *r = msg + 7;
            if (strcmp(r, "HIT") == 0)
                printf(">>> Hit!\n");
            else if (strcmp(r, "MISS") == 0)
                printf(">>> Miss.\n");
            else if (strcmp(r, "SUNK") == 0)
                printf(">>> Hit — ship sunk!\n");
            else
                printf(">>> Result: %s\n", r);
            continue;
        }
        if (strncmp(msg, "OPPONENT_RESULT ", 16) == 0) {
            char kind[16];
            int rr = -1, cc = -1;
            if (sscanf(msg + 16, "%15s %d %d", kind, &rr, &cc) == 3 && rr >= 0 && cc >= 0) {
                printf("Opponent fired at %c%d: %s\n", 'A' + rr, cc, kind);
            } else
                printf("%s\n", msg);
            continue;
        }
        if (strcmp(msg, "BOARD_TRACKING") == 0) {
            if (recv_board(sock, board_track) != 0)
                goto out;
            print_grid("Your tracking board (opponent's ocean)", board_track);
            continue;
        }
        if (strcmp(msg, "BOARD_OWN") == 0) {
            if (recv_board(sock, board_own) != 0)
                goto out;
            print_grid("Your fleet", board_own);
            continue;
        }
        if (strncmp(msg, "GAME_OVER ", 10) == 0) {
            printf("\n*** %s ***\n", msg + 10);
            break;
        }

        printf("(server) %s\n", msg);
    }

out:
    close(sock);
    return 0;
}
