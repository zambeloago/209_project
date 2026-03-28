/*
 * Battleship server: accepts two TCP clients, coordinates setup and turn-based play.
 * Protocol: newline-terminated text messages (see comments near send_* helpers).
 */

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define GRID_N 10
#define NUM_SHIPS 5
#define MAX_LINE 512
#define LISTEN_BACKLOG 2

/* Standard fleet: lengths 2, 3, 3, 4, 5 */
static const int SHIP_LENS[NUM_SHIPS] = {2, 3, 3, 4, 5};

/* -------------------------------------------------------------------------- */
/* Structs: grid-backed player state and ships                                */
/* -------------------------------------------------------------------------- */

typedef struct Ship {
    int len;
    int r0, c0, r1, c1; /* inclusive endpoints, straight horizontal or vertical */
    int hits;            /* number of distinct cells hit on this ship */
} Ship;

typedef struct Player {
    char own[GRID_N][GRID_N];    /* '.' water, 'S' ship, 'X' hit on ship, 'O' opp miss */
    char track[GRID_N][GRID_N]; /* '-' unknown, 'X' hit, 'O' miss (on opponent) */
    Ship ships[NUM_SHIPS];
    int num_placed;
} Player;

typedef struct Game {
    Player p[2];
    int fds[2];
} Game;

/* -------------------------------------------------------------------------- */
/* Socket I/O helpers (TCP stream-safe line handling)                         */
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

/* Read one line into buf (without trailing \r\n). Returns -1 on error/EOF. */
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

/* -------------------------------------------------------------------------- */
/* Parsing: single cell "A0".."J9" and range "B2-B4"                          */
/* -------------------------------------------------------------------------- */

static int parse_cell(const char *s, int *row, int *col) {
    if (!s || !*s)
        return -1;
    unsigned char ch = (unsigned char)toupper((unsigned char)s[0]);
    if (ch < 'A' || ch > 'J')
        return -1;
    *row = ch - 'A';
    char *end = NULL;
    long col_l = strtol(s + 1, &end, 10);
    if (end == s + 1 || *end != '\0')
        return -1;
    if (col_l < 0 || col_l >= GRID_N)
        return -1;
    *col = (int)col_l;
    return 0;
}

/* Trim spaces in place */
static void trim_inplace(char *s) {
    char *p = s;
    while (*p && isspace((unsigned char)*p))
        p++;
    if (p != s)
        memmove(s, p, strlen(p) + 1);
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1]))
        n--;
    s[n] = '\0';
}

/*
 * Parse "B2-B4" or "B2 - B4". Sets endpoints and implied length (number of cells).
 * Returns 0 on success.
 */
static int parse_placement(const char *line, int *r0, int *c0, int *r1, int *c1, int *seglen) {
    char buf[MAX_LINE];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    trim_inplace(buf);

    char *dash = strchr(buf, '-');
    if (!dash || dash == buf || !dash[1])
        return -1;
    *dash = '\0';
    char *left = buf;
    char *right = dash + 1;
    trim_inplace(left);
    trim_inplace(right);

    if (parse_cell(left, r0, c0) != 0 || parse_cell(right, r1, c1) != 0)
        return -1;

    int dr = abs(*r1 - *r0);
    int dc = abs(*c1 - *c0);
    if ((dr != 0 && dc != 0) || (dr == 0 && dc == 0))
        return -1;
    *seglen = dr + dc + 1;
    return 0;
}

/* -------------------------------------------------------------------------- */
/* Board and ship helpers                                                     */
/* -------------------------------------------------------------------------- */

static void player_init(Player *pl) {
    for (int r = 0; r < GRID_N; r++) {
        for (int c = 0; c < GRID_N; c++) {
            pl->own[r][c] = '.';
            pl->track[r][c] = '-';
        }
    }
    pl->num_placed = 0;
}

static int cell_on_ship_segment(int r, int c, const Ship *sh) {
    if (sh->r0 == sh->r1)
        return r == sh->r0 && c >= sh->c0 && c <= sh->c1;
    return c == sh->c0 && r >= sh->r0 && r <= sh->r1;
}

static int ship_fully_hit(const Player *def, const Ship *sh) {
    int r0 = sh->r0, r1 = sh->r1, c0 = sh->c0, c1 = sh->c1;
    if (r0 == r1) {
        int r = r0;
        int a = c0 < c1 ? c0 : c1;
        int b = c0 < c1 ? c1 : c0;
        for (int c = a; c <= b; c++) {
            if (def->own[r][c] != 'X')
                return 0;
        }
    } else {
        int c = c0;
        int a = r0 < r1 ? r0 : r1;
        int b = r0 < r1 ? r1 : r0;
        for (int r = a; r <= b; r++) {
            if (def->own[r][c] != 'X')
                return 0;
        }
    }
    return 1;
}

static int count_remaining_ship_cells(const Player *def) {
    int n = 0;
    for (int r = 0; r < GRID_N; r++)
        for (int c = 0; c < GRID_N; c++)
            if (def->own[r][c] == 'S')
                n++;
    return n;
}

static Ship *find_ship_covering(Player *def, int r, int c) {
    for (int i = 0; i < def->num_placed; i++) {
        if (cell_on_ship_segment(r, c, &def->ships[i]))
            return &def->ships[i];
    }
    return NULL;
}

static void recompute_ship_hits(Player *def) {
    for (int i = 0; i < def->num_placed; i++) {
        Ship *sh = &def->ships[i];
        int h = 0;
        int r0 = sh->r0, r1 = sh->r1, c0 = sh->c0, c1 = sh->c1;
        if (r0 == r1) {
            int r = r0;
            int a = c0 < c1 ? c0 : c1;
            int b = c0 < c1 ? c1 : c0;
            for (int c = a; c <= b; c++) {
                if (def->own[r][c] == 'X')
                    h++;
            }
        } else {
            int c = c0;
            int a = r0 < r1 ? r0 : r1;
            int b = r0 < r1 ? r1 : r0;
            for (int r = a; r <= b; r++) {
                if (def->own[r][c] == 'X')
                    h++;
            }
        }
        sh->hits = h;
    }
}

/* -------------------------------------------------------------------------- */
/* Placement validation and application                                       */
/* -------------------------------------------------------------------------- */

static int placement_ok(Player *pl, int r0, int c0, int r1, int c1, int need_len, char *err,
                        size_t errcap) {
    int dr = abs(r1 - r0);
    int dc = abs(c1 - c0);
    if ((dr != 0 && dc != 0) || (dr == 0 && dc == 0)) {
        snprintf(err, errcap, "Ship must be a straight horizontal or vertical line.");
        return 0;
    }
    int seglen = dr + dc + 1;
    if (seglen != need_len) {
        snprintf(err, errcap, "Wrong length: need %d cells, got %d.", need_len, seglen);
        return 0;
    }
    int lo_r = r0 < r1 ? r0 : r1;
    int hi_r = r0 < r1 ? r1 : r0;
    int lo_c = c0 < c1 ? c0 : c1;
    int hi_c = c0 < c1 ? c1 : c0;
    if (lo_r < 0 || hi_r >= GRID_N || lo_c < 0 || hi_c >= GRID_N) {
        snprintf(err, errcap, "Placement out of bounds.");
        return 0;
    }
    for (int r = lo_r; r <= hi_r; r++) {
        for (int c = lo_c; c <= hi_c; c++) {
            if (pl->own[r][c] != '.') {
                snprintf(err, errcap, "Overlaps another ship.");
                return 0;
            }
        }
    }
    return 1;
}

static void apply_placement(Player *pl, int idx, int r0, int c0, int r1, int c1) {
    Ship *sh = &pl->ships[idx];
    sh->r0 = r0;
    sh->c0 = c0;
    sh->r1 = r1;
    sh->c1 = c1;
    sh->len = abs(r1 - r0) + abs(c1 - c0) + 1;
    sh->hits = 0;

    int lo_r = r0 < r1 ? r0 : r1;
    int hi_r = r0 < r1 ? r1 : r0;
    int lo_c = c0 < c1 ? c0 : c1;
    int hi_c = c0 < c1 ? c1 : c0;
    for (int r = lo_r; r <= hi_r; r++) {
        for (int c = lo_c; c <= hi_c; c++)
            pl->own[r][c] = 'S';
    }
    pl->num_placed++;
}

/* -------------------------------------------------------------------------- */
/* Server bootstrap                                                           */
/* -------------------------------------------------------------------------- */

static int setup_server(uint16_t port) {
    int listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_socket == -1) {
        perror("socket");
        exit(1);
    }
    int opt = 1;
    setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listen_socket, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(listen_socket);
        exit(1);
    }
    if (listen(listen_socket, LISTEN_BACKLOG) == -1) {
        perror("listen");
        close(listen_socket);
        exit(1);
    }
    printf("Server listening on port %u\n", (unsigned)port);
    return listen_socket;
}

static void notify_game_over(Game *g, int winner) {
    for (int p = 0; p < 2; p++) {
        if (p == winner)
            send_line(g->fds[p], "GAME_OVER YOU_WIN");
        else
            send_line(g->fds[p], "GAME_OVER YOU_LOSE");
    }
}

static void close_both(Game *g) {
    for (int p = 0; p < 2; p++) {
        if (g->fds[p] >= 0) {
            shutdown(g->fds[p], SHUT_RDWR);
            close(g->fds[p]);
            g->fds[p] = -1;
        }
    }
}

/* After header, send GRID_N rows as separate lines (each row is GRID_N chars). */
static int send_board_block(int fd, const char *header, char board[GRID_N][GRID_N]) {
    if (send_line(fd, header) != 0)
        return -1;
    for (int r = 0; r < GRID_N; r++) {
        char line[GRID_N + 1];
        memcpy(line, board[r], (size_t)GRID_N);
        line[GRID_N] = '\0';
        if (send_line(fd, line) != 0)
            return -1;
    }
    return 0;
}

static int run_setup_phase(Game *g) {
    for (int player = 0; player < 2; player++) {
        int fd = g->fds[player];
        for (int s = 0; s < NUM_SHIPS; s++) {
            char prompt[64];
            snprintf(prompt, sizeof(prompt), "PLACE %d %d", s, SHIP_LENS[s]);
            for (;;) {
                if (send_line(fd, prompt) != 0)
                    return -1;
                char line[MAX_LINE];
                if (recv_line(fd, line, sizeof(line)) < 0)
                    return -1;

                int r0, c0, r1, c1, seglen;
                if (parse_placement(line, &r0, &c0, &r1, &c1, &seglen) != 0) {
                    send_line(fd, "ERR Use format like B2-B4 (horizontal or vertical).");
                    continue;
                }
                char err[128];
                if (!placement_ok(&g->p[player], r0, c0, r1, c1, SHIP_LENS[s], err, sizeof(err))) {
                    char msg[MAX_LINE];
                    snprintf(msg, sizeof(msg), "ERR %s", err);
                    send_line(fd, msg);
                    continue;
                }
                apply_placement(&g->p[player], s, r0, c0, r1, c1);
                if (send_line(fd, "PLACE_OK") != 0)
                    return -1;
                break;
            }
        }
    }
    return 0;
}

static int run_game_loop(Game *g) {
    int turn = 0; /* attacker index */

    for (;;) {
        int atk = turn;
        int def = 1 - atk;

        if (send_line(g->fds[atk], "YOUR_TURN") != 0)
            return -1;
        if (send_line(g->fds[def], "WAIT_TURN") != 0)
            return -1;

        int r = 0, c = 0;
        for (;;) {
            char line[MAX_LINE];
            if (recv_line(g->fds[atk], line, sizeof(line)) < 0)
                return -1;
            trim_inplace(line);

            if (parse_cell(line, &r, &c) != 0) {
                if (send_line(g->fds[atk], "ERR_BAD_SHOT") != 0)
                    return -1;
                continue;
            }

            if (g->p[atk].track[r][c] != '-') {
                if (send_line(g->fds[atk], "ERR_ALREADY_GUESSED") != 0)
                    return -1;
                continue;
            }
            break;
        }

        int hit = (g->p[def].own[r][c] == 'S');
        int was_sunk = 0;

        if (hit) {
            g->p[def].own[r][c] = 'X';
            g->p[atk].track[r][c] = 'X';
            recompute_ship_hits(&g->p[def]);
            Ship *sh = find_ship_covering(&g->p[def], r, c);
            if (sh && ship_fully_hit(&g->p[def], sh))
                was_sunk = 1;
        } else if (g->p[def].own[r][c] == '.' || g->p[def].own[r][c] == 'O') {
            g->p[def].own[r][c] = 'O';
            g->p[atk].track[r][c] = 'O';
        } else {
            /* Defensive: treat unknown state as miss on tracking */
            g->p[atk].track[r][c] = 'O';
        }

        if (was_sunk)
            send_line(g->fds[atk], "RESULT SUNK");
        else if (hit)
            send_line(g->fds[atk], "RESULT HIT");
        else
            send_line(g->fds[atk], "RESULT MISS");

        char defmsg[64];
        snprintf(defmsg, sizeof(defmsg), "OPPONENT_RESULT %s %d %d", hit ? "HIT" : "MISS", r, c);
        if (send_line(g->fds[def], defmsg) != 0)
            return -1;

        if (send_board_block(g->fds[atk], "BOARD_TRACKING", g->p[atk].track) != 0)
            return -1;
        if (send_board_block(g->fds[def], "BOARD_OWN", g->p[def].own) != 0)
            return -1;
        /* Both players see their tracking grid after every resolved shot */
        if (send_board_block(g->fds[def], "BOARD_TRACKING", g->p[def].track) != 0)
            return -1;

        if (count_remaining_ship_cells(&g->p[def]) == 0) {
            notify_game_over(g, atk);
            return 0;
        }

        turn = def;
    }
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }
    char *end = NULL;
    long port_l = strtol(argv[1], &end, 10);
    if (!argv[1][0] || end == argv[1] || *end != '\0' || port_l < 1 || port_l > 65535) {
        fprintf(stderr, "Invalid port.\n");
        return 1;
    }

    int listen_fd = setup_server((uint16_t)port_l);
    Game g;
    g.fds[0] = g.fds[1] = -1;
    player_init(&g.p[0]);
    player_init(&g.p[1]);

    struct sockaddr_in peer;
    socklen_t plen = sizeof(peer);

    int a = accept(listen_fd, (struct sockaddr *)&peer, &plen);
    if (a < 0) {
        perror("accept");
        close(listen_fd);
        return 1;
    }
    g.fds[0] = a;
    char ip0[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &peer.sin_addr, ip0, sizeof(ip0));
    printf("Player 0 connected from %s\n", ip0);

    if (send_line(g.fds[0], "WELCOME 0") != 0 || send_line(g.fds[0], "WAIT_PEER") != 0) {
        perror("send");
        close_both(&g);
        close(listen_fd);
        return 1;
    }

    int b = accept(listen_fd, (struct sockaddr *)&peer, &plen);
    if (b < 0) {
        perror("accept");
        close_both(&g);
        close(listen_fd);
        return 1;
    }
    g.fds[1] = b;
    char ip1[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &peer.sin_addr, ip1, sizeof(ip1));
    printf("Player 1 connected from %s\n", ip1);

    close(listen_fd);

    if (send_line(g.fds[1], "WELCOME 1") != 0 || send_line(g.fds[0], "PEER_READY") != 0) {
        close_both(&g);
        return 1;
    }

    if (run_setup_phase(&g) != 0) {
        fprintf(stderr, "Setup failed (disconnect or I/O error).\n");
        close_both(&g);
        return 1;
    }

    if (send_line(g.fds[0], "SETUP_DONE") != 0 || send_line(g.fds[1], "SETUP_DONE") != 0) {
        close_both(&g);
        return 1;
    }

    printf("Both players ready. Game on.\n");

    if (run_game_loop(&g) != 0) {
        fprintf(stderr, "Game aborted.\n");
        close_both(&g);
        return 1;
    }

    close_both(&g);
    printf("Game finished. Connections closed.\n");
    return 0;
}
