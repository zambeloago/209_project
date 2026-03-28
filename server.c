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
#include "game.h"
#include "utils.h"

/* ------------------------------------------------------------------ */
/* Data structures                                                      */
/* ------------------------------------------------------------------ */

typedef struct {
    int fd;         /* socket file descriptor; -1 = unused slot */
    int in_game;    /* 1 once the game has started              */
    int player_id;  /* 0 or 1                                   */
    char username[64];
} client_t;

/* ------------------------------------------------------------------ */
/* Global state                                                         */
/* ------------------------------------------------------------------ */

static client_t clients[MAX_CLIENTS];
static game_t   game;
static int      client_count = 0; /* number of currently connected clients */

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

/* Send a newline-terminated message to a single fd. */
static void send_msg(int fd, const char *msg) {
    char buf[BUFFER_SIZE];
    snprintf(buf, sizeof(buf), "%s\n", msg);
    if (send(fd, buf, strlen(buf), 0) < 0)
        perror("send");
}

/* Initialise the clients array. */
static void init_clients(void) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd        = -1;
        clients[i].in_game   = 0;
        clients[i].player_id = -1;
        clients[i].username[0] = '\0';
    }
}

/* Return the index of the first free slot, or -1 if full. */
static int find_free_slot(void) {
    for (int i = 0; i < MAX_CLIENTS; i++)
        if (clients[i].fd == -1)
            return i;
    return -1;
}

/* Return the index of the client with player_id == pid, or -1. */
static int find_player(int pid) {
    for (int i = 0; i < MAX_CLIENTS; i++)
        if (clients[i].fd != -1 && clients[i].player_id == pid)
            return i;
    return -1;
}

/*
 * Remove client at index idx: close socket, zero slot, decrement counter,
 * and reset game state if a game was active.
 */
static void remove_client(int idx) {
    if (clients[idx].fd == -1) return;

    printf("[server] Client (fd=%d, player=%d) disconnected.\n",
           clients[idx].fd, clients[idx].player_id);

    close(clients[idx].fd);

    int was_in_game = clients[idx].in_game;

    clients[idx].fd        = -1;
    clients[idx].in_game   = 0;
    clients[idx].player_id = -1;
    clients[idx].username[0] = '\0';
    client_count--;

    /* If a game was running, notify the remaining player and reset. */
    if (was_in_game && game.active) {
        game.active = 0;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd != -1 && clients[i].in_game) {
                send_msg(clients[i].fd,
                         "MESSAGE Opponent disconnected. Game over.");
                clients[i].in_game   = 0;
                clients[i].player_id = -1;
            }
        }
    }
}

/* Start the game once 2 players are connected. */
static void try_start_game(void) {
    if (client_count < 2 || game.active) return;

    /* Assign player IDs to the first two connected clients */
    int assigned = 0;
    for (int i = 0; i < MAX_CLIENTS && assigned < 2; i++) {
        if (clients[i].fd != -1 && !clients[i].in_game) {
            clients[i].player_id = assigned;
            clients[i].in_game   = 1;
            assigned++;
        }
    }
    if (assigned < 2) return; /* shouldn't happen, but be safe */

    init_game(&game);

    printf("[server] Game started between player 0 and player 1.\n");

    /* Notify both players */
    int p0 = find_player(0);
    int p1 = find_player(1);
    send_msg(clients[p0].fd, MSG_START);
    send_msg(clients[p1].fd, MSG_START);

    /* Give the first turn to player 0 */
    send_msg(clients[p0].fd, MSG_TURN);
    send_msg(clients[p1].fd, "MESSAGE Waiting for opponent's move...");
}

/* Handle a MOVE message from the client at index idx. */
static void handle_move(int idx, const char *buf) {
    int player = clients[idx].player_id;

    if (!game.active) {
        send_msg(clients[idx].fd, MSG_INVALID);
        return;
    }

    /* It must be this player's turn */
    if (game.current_turn != player) {
        send_msg(clients[idx].fd, MSG_INVALID);
        return;
    }

    int row, col;
    if (!parse_move(buf, &row, &col)) {
        send_msg(clients[idx].fd, MSG_INVALID);
        return;
    }

    int result = process_move(&game, player, row, col);

    if (result == -1) {
        send_msg(clients[idx].fd, MSG_INVALID);
        return;
    }

    if (result == 1) {
        /* HIT – player keeps their turn */
        send_msg(clients[idx].fd, MSG_HIT);
        printf("[server] Player %d HIT at (%d,%d).\n", player, row, col);

        /* Check for win */
        int winner = check_win(&game);
        if (winner != -1) {
            int w_idx = find_player(winner);
            int l_idx = find_player(1 - winner);
            if (w_idx != -1) send_msg(clients[w_idx].fd, MSG_WIN);
            if (l_idx != -1) send_msg(clients[l_idx].fd, MSG_LOSE);
            game.active = 0;
            /* Reset in_game flags */
            for (int i = 0; i < MAX_CLIENTS; i++)
                clients[i].in_game = 0;
            printf("[server] Player %d wins!\n", winner);
        } else {
            /* Continue turn: prompt player again */
            send_msg(clients[idx].fd, MSG_TURN);
        }
    } else {
        /* MISS – switch turn */
        send_msg(clients[idx].fd, MSG_MISS);
        printf("[server] Player %d MISS at (%d,%d).\n", player, row, col);

        switch_turn(&game);
        int next = find_player(game.current_turn);
        int cur  = find_player(1 - game.current_turn);
        if (next != -1) send_msg(clients[next].fd, MSG_TURN);
        if (cur  != -1) send_msg(clients[cur].fd,
                                 "MESSAGE Opponent's turn. Please wait...");
    }
}

/* Dispatch an incoming message from client idx. */
static void handle_message(int idx, char *buf) {
    trim_newline(buf);
    printf("[server] Received from player %d: \"%s\"\n",
           clients[idx].player_id, buf);

    if (strncmp(buf, MSG_JOIN, strlen(MSG_JOIN)) == 0) {
        /* Optional: store username */
        const char *name = buf + strlen(MSG_JOIN);
        while (*name == ' ') name++;
        if (*name)
            strncpy(clients[idx].username, name,
                    sizeof(clients[idx].username) - 1);
        /* After JOIN, trigger game start check */
        try_start_game();
    } else if (strncmp(buf, MSG_MOVE, strlen(MSG_MOVE)) == 0) {
        handle_move(idx, buf);
    } else {
        send_msg(clients[idx].fd, MSG_INVALID);
    }
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

int main(void) {
    int server_fd;
    struct sockaddr_in server_addr;

    init_clients();
    memset(&game, 0, sizeof(game));

    /* Create TCP socket */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); exit(EXIT_FAILURE); }

    /* Allow port reuse to avoid "Address already in use" during testing */
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt"); exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0) {
        perror("bind"); exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen"); exit(EXIT_FAILURE);
    }

    printf("[server] Listening on port %d...\n", PORT);

    /* ---- select() event loop ---- */
    fd_set read_fds;
    int max_fd;

    for (;;) {
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);
        max_fd = server_fd;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd != -1) {
                FD_SET(clients[i].fd, &read_fds);
                if (clients[i].fd > max_fd)
                    max_fd = clients[i].fd;
            }
        }

        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (activity < 0) {
            if (errno == EINTR) continue; /* interrupted by signal, retry */
            perror("select");
            break;
        }

        /* New connection? */
        if (FD_ISSET(server_fd, &read_fds)) {
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            int new_fd = accept(server_fd,
                                (struct sockaddr *)&client_addr, &addr_len);
            if (new_fd < 0) {
                perror("accept");
            } else {
                int slot = find_free_slot();
                if (slot == -1 || client_count >= MAX_CLIENTS) {
                    /* Server full */
                    const char *full = "MESSAGE Server full.\n";
                    send(new_fd, full, strlen(full), 0);
                    close(new_fd);
                    printf("[server] Rejected connection – server full.\n");
                } else {
                    clients[slot].fd        = new_fd;
                    clients[slot].in_game   = 0;
                    clients[slot].player_id = -1;
                    clients[slot].username[0] = '\0';
                    client_count++;
                    printf("[server] New client (fd=%d) connected. "
                           "Total: %d\n", new_fd, client_count);
                    send_msg(new_fd, MSG_WAIT);

                    /* If this is the 2nd client and no game running, start */
                    if (client_count == 2 && !game.active)
                        try_start_game();
                }
            }
        }

        /* Data from existing clients? */
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd == -1) continue;
            if (!FD_ISSET(clients[i].fd, &read_fds)) continue;

            char buf[BUFFER_SIZE];
            memset(buf, 0, sizeof(buf));
            int bytes = recv(clients[i].fd, buf, sizeof(buf) - 1, 0);

            if (bytes <= 0) {
                /* Connection closed or error */
                remove_client(i);
            } else {
                buf[bytes] = '\0';
                handle_message(i, buf);
            }
        }
    }

    close(server_fd);
    return 0;
}