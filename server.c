#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

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
    q
    printf("Server listening on port %d\n", port);
    return listen_socket;
}

// ── main loop ─────────────────────────────────────────────────────
void main_loop(int listen_fd) {
    fd_set read_fds;
    while (1) {
        FD_ZERO(&read_fds);
        // TODO: FD_SET listen_fd and each active player fd
        // TODO: compute max_fd
        // TODO: select(max_fd + 1, &read_fds, NULL, NULL, NULL)
        // TODO: if listen_fd ready      -> handle_new_connection(listen_fd)
        // TODO: if p[0].fd ready        -> handle_client_message(0)
        // TODO: if p[1].fd ready        -> handle_client_message(1)
    }
}

// connection code
void handle_new_connection(int listen_fd) {
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    // TODO: accept(), reject if no free slot
    int fd = accept(listen_fd, (struct sockaddr*) &addr, &addrlen);
    if (fd == -1) {
        perror("accept");
        return;
    }

    // find a free slot for new connection
    int slot = -1;
    if (game.p[0].fd == -1) {
        slot = 0
    } else if (game.p[1].fd == -1) {
        slot = 1
    }

    if (slot == -1) {
        send_msg(game.p[0].fd, game.p[1].fd);
        close(fd);
    }
    // TODO: assign fd to free slot, send "WELCOME <slot>\n"

    game.p[slot].fd      = fd;
    game.p[slot].buf_len = 0;

    send_msg(fd, "WELCOME %d\n", slot);
    send_msg(fd, "WAIT\n");

    // TODO: if both slots filled: game.state = PLACING, send "PLACING\n" to both

    if (game.p[0].fd != -1 && game.p[1].fd != -1) {
        game.state = PLACING;
        send_msg(game.p[0].fd, "PLACING\n");
        send_msg(game.p[1].fd, "PLACING\n");
    }

}

void handle_disconnect(int player_idx) {
    // TODO: close fd, set fd = -1
    close(game.p[player_idx].fd);
    // TODO: notify other player "ERR opponent disconnected\n"
    int other = 1 - player_idx;
    if (game.p[other].fd != -1) {
        send_msg(game.p[other].fd, "ERR opponent disconnected\n");
    }
    // TODO: reset_game(&game), game.state = WAITING
    reset_game(&game);
    game.state = WAITING;
}

// message handling
void handle_client_message(int player_idx) {
    Player *p = &game.p[player_idx];
    // TODO: recv() into p->buf + p->buf_len
    int n = recv(p->fd, p->buf + p->buf_len, BUF_SIZE - p->buf_len - 1, 0);
    // TODO: if n <= 0: handle_disconnect(player_idx), return
    if (n <= 0) {
        handle_disconnect(player_idx);
        return;
    }
    // TODO: update p->buf_len, null-terminate
    p->buf_len += n;
    p->buf[p->buf_len] = '\0';

    char *start = p->buf;
    char *nl;
    while ((nl = strchr(start, '\n')) != NULL) {
        *nl = '\0';
        dispatch_command(player_idx, start);
        start = nl + 1;
    }
    // TODO: memmove remaining bytes to front of buf, update buf_len
    p->buf_len = (p->buf + p->buf_len) - start;
    memmove(p->buf, start, p->buf_len);
}

void dispatch_command(int player_idx, char *line) {
    trim_newline(line);
    char *cmd = strtok(line, " ");
    if (!cmd) return;
    // TODO: strcmp cmd and route to handler
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
        handle_fire(player_idx, strtok(NULL, " "));
    } else if (strcmp(cmd, "REMATCH") == 0) {
        handle_rematch(player_idx);
    } else if (strcmp(cmd, "QUIT")    == 0) {
        handle_disconnect(player_idx);
    } else {
        send_msg(fd, "ERR unknown command\n");
    }
}

// command handlers
void handle_login(int player_idx, char *name) {
    // TODO: store name, send "OK\n"
}

void handle_place(int player_idx, char *coords, char dir, int len) {
    // TODO: validate state, coords, dir, len
    // TODO: can_place_ship() -> place_ship(), ships_placed++, send "OK\n"
}

void handle_ready(int player_idx) {
    // TODO: check ships_placed == MAX_SHIPS, set ready = 1
    // TODO: if both ready: state = P1_TURN, send "YOUR_TURN\n"/"OPP_TURN\n"
    // TODO: else send "WAIT\n"
}

void handle_fire(int player_idx, char *coords) {
    // TODO: check it's this player's turn
    // TODO: apply_shot() -> send "HIT"/"MISS" to shooter, "OPP_HIT"/"OPP_MISS" to target
    // TODO: if sunk: send "SUNK\n"/"OPP_SUNK\n"
    // TODO: if win:  send "WIN\n"/"LOSE\n", state = GAME_OVER, return
    // TODO: switch turns, send "YOUR_TURN\n"/"OPP_TURN\n"
}

void handle_rematch(int player_idx) {
    // TODO: set wants_rematch = 1
    // TODO: if both want rematch: reset_game(), send "PLACING\n" to both
    // TODO: else send "WAIT\n"
}

//
int main(int argc, char *argv[]) {
    if (argc != 2) { fprintf(stderr, "usage: %s <port>\n", argv[0]); exit(1); }
    game.state    = WAITING;
    game.p[0].fd  = -1;
    game.p[1].fd  = -1;
    int listen_fd = setup_server(atoi(argv[1]));
    main_loop(listen_fd);
    return 0;
}
