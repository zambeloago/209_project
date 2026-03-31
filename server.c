#include "common.h"
#include "game.h"
#include "utils.h"

// Protocol: line-based. Shots reported as "HIT ColRow" / "MISS ColRow" (e.g. HIT E5). Grid chars in game.c: . S X M.
// BOARD_OWN + 100 chars: rows 0..9 of own_grid concatenated (client shows ships as 'S').

Game game;

static void send_board_own(int fd, const Player *p) {
    char line[10 + GRID_SIZE * GRID_SIZE + 2];
    memcpy(line, "BOARD_OWN ", 10);
    int k = 10;
    for (int r = 0; r < GRID_SIZE; r++) {
        memcpy(line + k, p->own_grid[r], GRID_SIZE);
        k += GRID_SIZE;
    }
    line[k++] = '\n';
    line[k] = '\0';
    send_msg(fd, "%s", line);
}

void main_loop(int listen_fd);
void handle_new_connection(int listen_fd);
void handle_disconnect(int player_idx);
void handle_client_message(int player_idx);
void dispatch_command(int player_idx, char *line);
void handle_login(int player_idx, char *name);
void handle_place(int player_idx, char *coords, char dir, int len);
void handle_ready(int player_idx);
void handle_fire(int player_idx, char *coords);
void handle_rematch(int player_idx);

// setup
int setup_server(int port) {
    int listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_socket == -1) {
        //socket error handling
        perror("socket");
        exit(1);
    }
    int opt = 1;
    setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = INADDR_ANY;

    if ( bind(listen_socket, (struct sockaddr*) &server_address, sizeof(server_address)) == -1) {
        perror("bind");
        close(listen_socket);
        exit(1);
        //remember to set up a socket and bind on client code
    }

    if (listen(listen_socket, 2) == -1) {
        perror("listen");
        close(listen_socket);
        exit(1);
    }

    printf("Server listening on port %d\n", port);
    return listen_socket;
}

// ── main loop ─────────────────────────────────────────────────────
void main_loop(int listen_fd) {
    fd_set read_fds;
    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(listen_fd, &read_fds);
        int max_fd = listen_fd;

        if (game.p[0].fd != -1) {
            FD_SET(game.p[0].fd, &read_fds);
            if (game.p[0].fd > max_fd) max_fd = game.p[0].fd;
        }
        if (game.p[1].fd != -1) {
            FD_SET(game.p[1].fd, &read_fds);
            if (game.p[1].fd > max_fd) max_fd = game.p[1].fd;
        }

        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) {
            perror("select");
            continue;
        }

        if (FD_ISSET(listen_fd, &read_fds)) handle_new_connection(listen_fd);
        if (game.p[0].fd != -1 && FD_ISSET(game.p[0].fd, &read_fds)) handle_client_message(0);
        if (game.p[1].fd != -1 && FD_ISSET(game.p[1].fd, &read_fds)) handle_client_message(1);
    }
}

// connection code
void handle_new_connection(int listen_fd) {
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    int fd = accept(listen_fd, (struct sockaddr*) &addr, &addrlen);
    if (fd == -1) {
        perror("accept");
        return;
    }

    // find a free slot for new connection
    int slot = -1;
    if (game.p[0].fd == -1) {
        slot = 0;
    } else if (game.p[1].fd == -1) {
        slot = 1;
    }

    if (slot == -1) {
        send_msg(fd, "ERR server full\n");
        close(fd);
        return;
    }

    game.p[slot].fd      = fd;
    game.p[slot].buf_len = 0;
    memset(game.p[slot].buf, 0, sizeof(game.p[slot].buf));
    game.p[slot].name[0] = '\0';

    send_msg(fd, "WELCOME %d\n", slot);
    send_msg(fd, "WAIT\n");

    if (game.p[0].fd != -1 && game.p[1].fd != -1) {
        int fd0 = game.p[0].fd, fd1 = game.p[1].fd;
        reset_game(&game);
        game.p[0].fd = fd0;
        game.p[1].fd = fd1;
        send_msg(fd0, "PLACING\n");
        send_msg(fd1, "PLACING\n");
        send_board_own(fd0, &game.p[0]);
        send_board_own(fd1, &game.p[1]);
    }

}

void handle_disconnect(int player_idx) {
    if (game.p[player_idx].fd != -1) {
        close(game.p[player_idx].fd);
        game.p[player_idx].fd = -1;
    }

    int other = 1 - player_idx;
    int other_fd = game.p[other].fd;

    reset_game(&game);
    game.state = WAITING;
    game.p[other].fd = other_fd;

    if (other_fd != -1) {
        send_msg(other_fd, "ERR opponent disconnected\n");
        send_msg(other_fd, "WAIT\n");
    }
}

// message handling
void handle_client_message(int player_idx) {
    Player *p = &game.p[player_idx];
    if (p->buf_len >= BUF_SIZE - 1) {
        p->buf_len = 0;
        p->buf[0] = '\0';
        send_msg(p->fd, "ERR line too long\n");
        return;
    }

    int n = recv(p->fd, p->buf + p->buf_len, BUF_SIZE - p->buf_len - 1, 0);
    if (n <= 0) {
        handle_disconnect(player_idx);
        return;
    }

    p->buf_len += n;
    p->buf[p->buf_len] = '\0';

    char *start = p->buf;
    char *nl;
    while ((nl = strchr(start, '\n')) != NULL) {
        *nl = '\0';
        dispatch_command(player_idx, start);
        start = nl + 1;
    }
    p->buf_len = (p->buf + p->buf_len) - start;
    memmove(p->buf, start, p->buf_len);
    p->buf[p->buf_len] = '\0';
}

void dispatch_command(int player_idx, char *line) {
    trim_newline(line);
    char *cmd = strtok(line, " ");
    if (!cmd) return;

    int fd = game.p[player_idx].fd;

    if (strcmp(cmd, "LOGIN")   == 0) {
        handle_login(player_idx, strtok(NULL, " "));
    } else if (strcmp(cmd, "PLACE")   == 0) {
        char *coords = strtok(NULL, " ");
        char *dir_s  = strtok(NULL, " ");
        char *len_s  = strtok(NULL, " ");
        if (!coords || !dir_s || !len_s) { send_msg(fd, "ERR bad args\n"); return; }
        handle_place(player_idx, coords, dir_s[0], atoi(len_s));
    } else if (strcmp(cmd, "READY")   == 0) {
        handle_ready(player_idx);
    } else if (strcmp(cmd, "FIRE")    == 0) {
        char *coords = strtok(NULL, " ");
        if (!coords) { send_msg(fd, "ERR bad args\n"); return; }
        handle_fire(player_idx, coords);
    } else if (strcmp(cmd, "REMATCH") == 0) {
        handle_rematch(player_idx);
    } else if (strcmp(cmd, "QUIT")    == 0) {
        handle_disconnect(player_idx);
    } else {
        send_msg(fd, "ERR unknown command\n");
    }
}

// command handlers

// LOGIN only saves a display name on the Player; nothing in this server reads .name for rules or I/O.
// Optional for clients; gameplay does not require it.
void handle_login(int player_idx, char *name) {
    int fd = game.p[player_idx].fd;
    if (!name || !*name) {
        send_msg(fd, "ERR bad args\n");
        return;
    }

    strncpy(game.p[player_idx].name, name, NAME_LEN - 1);
    game.p[player_idx].name[NAME_LEN - 1] = '\0';
    send_msg(fd, "OK\n");
}

void handle_place(int player_idx, char *coords, char dir, int len) {
    Player *p = &game.p[player_idx];
    if (game.state != PLACING) {
        send_msg(p->fd, "ERR not in placing phase\n");
        return;
    }
    if (p->ships_placed >= MAX_SHIPS) {
        send_msg(p->fd, "ERR all ships already placed\n");
        return;
    }

    int row, col;
    if (!parse_coords(coords, &row, &col)) {
        send_msg(p->fd, "ERR bad coords\n");
        return;
    }
    if (len < 2 || len > 5) {
        send_msg(p->fd, "ERR ship length must be 2-5 (fleet is 2,3,3,4,5)\n");
        return;
    }
    if (p->ships_left_by_len[len] <= 0) {
        send_msg(p->fd, "ERR no ship of that length left to place\n");
        return;
    }
    if (!can_place_ship(p->own_grid, p->ship_id, row, col, dir, len)) {
        send_msg(p->fd, "ERR invalid placement\n");
        return;
    }

    place_ship(p->own_grid, p->ship_id, row, col, dir, len);
    p->ships_left_by_len[len]--;
    p->ships_placed++;
    send_msg(p->fd, "OK\n");
    send_board_own(p->fd, p);
}

void handle_ready(int player_idx) {
    Player *p = &game.p[player_idx];
    if (game.state != PLACING) {
        send_msg(p->fd, "ERR not in placing phase\n");
        return;
    }
    if (p->ships_placed != MAX_SHIPS) {
        send_msg(p->fd, "ERR place all ships first\n");
        return;
    }

    p->ready = 1;
    if (game.p[0].ready && game.p[1].ready) {
        game.state = P1_TURN;
        send_msg(game.p[0].fd, "YOUR_TURN\n");
        send_msg(game.p[1].fd, "OPP_TURN\n");
    } else {
        send_msg(p->fd, "WAIT\n");
    }
}

void handle_fire(int player_idx, char *coords) {
    int shooter = player_idx;
    int target = 1 - shooter;
    int shooter_turn = (game.state == P1_TURN && shooter == 0) || (game.state == P2_TURN && shooter == 1);
    if (!shooter_turn) {
        send_msg(game.p[shooter].fd, "ERR not your turn\n");
        return;
    }

    int row, col;
    if (!parse_coords(coords, &row, &col)) {
        send_msg(game.p[shooter].fd, "ERR bad coords\n");
        return;
    }

    int result = apply_shot(&game, target, row, col);
    if (result == 0) {
        send_msg(game.p[shooter].fd, "ERR invalid shot\n");
        return;
    }

    // Protocol: still says "HIT" / "MISS" to client; server grids use 'X' / 'M' (same as game.c).
    if (result == 'X') {
        game.p[shooter].shot_grid[row][col] = 'X';
        send_msg(game.p[shooter].fd, "HIT %c%d\n", 'A' + col, row + 1);
        send_msg(game.p[target].fd, "OPP_HIT %c%d\n", 'A' + col, row + 1);

        if (is_ship_sunk(game.p[target].own_grid, game.p[target].ship_id, row, col)) {
            send_msg(game.p[shooter].fd, "SUNK\n");
            send_msg(game.p[target].fd, "OPP_SUNK\n");
        }

        if (check_win(&game, target)) {
            game.state = GAME_OVER;
            send_msg(game.p[shooter].fd, "WIN\n");
            send_msg(game.p[target].fd, "LOSE\n");
            return;
        }
    } else {
        game.p[shooter].shot_grid[row][col] = 'M';
        send_msg(game.p[shooter].fd, "MISS %c%d\n", 'A' + col, row + 1);
        send_msg(game.p[target].fd, "OPP_MISS %c%d\n", 'A' + col, row + 1);
    }

    game.state = (shooter == 0) ? P2_TURN : P1_TURN;
    send_msg(game.p[shooter].fd, "OPP_TURN\n");
    send_msg(game.p[target].fd, "YOUR_TURN\n");
}

void handle_rematch(int player_idx) {
    Player *p = &game.p[player_idx];
    if (game.state != GAME_OVER) {
        send_msg(p->fd, "ERR rematch only after game over\n");
        return;
    }

    p->wants_rematch = 1;
    if (game.p[0].wants_rematch && game.p[1].wants_rematch) {
        int fd0 = game.p[0].fd, fd1 = game.p[1].fd;
        reset_game(&game);
        game.p[0].fd = fd0;
        game.p[1].fd = fd1;
        send_msg(fd0, "PLACING\n");
        send_msg(fd1, "PLACING\n");
        send_board_own(fd0, &game.p[0]);
        send_board_own(fd1, &game.p[1]);
    } else {
        send_msg(p->fd, "WAIT\n");
    }
}

//
int main(int argc, char *argv[]) {
    if (argc != 2) { fprintf(stderr, "usage: %s <port>\n", argv[0]); exit(1); }
    reset_game(&game);
    game.state    = WAITING;
    game.p[0].fd  = -1;
    game.p[1].fd  = -1;
    int listen_fd = setup_server(atoi(argv[1]));
    main_loop(listen_fd);
    return 0;
}
