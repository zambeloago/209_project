#include "common.h"
#include "utils.h"

#define ANSI_RED   "\033[31m"
#define ANSI_GREEN "\033[32m"
#define ANSI_BLUE  "\033[34m"
#define ANSI_BOLD  "\033[1m"
#define ANSI_RESET "\033[0m"

int  sockfd;
char own_grid[GRID_SIZE][GRID_SIZE];
char shot_grid[GRID_SIZE][GRID_SIZE];


// setup
int connect_to_server(const char *host, int port) {
    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%d", port);
    // TODO: getaddrinfo(host, port_str, &hints, &res)
    // TODO: socket() + connect()
    // TODO: freeaddrinfo(res), return fd
    return -1;
}

// recv thread
void *recv_loop(void *arg) {
    char buf[BUF_SIZE];
    int  buf_len = 0;
    int  fd = *(int *)arg;
    while (1) {
        // TODO: recv() into buf + buf_len
        // TODO: if n <= 0: print disconnected, exit(0)
        // TODO: update buf_len, null-terminate

        char *start = buf, *nl;
        while ((nl = strchr(start, '\n')) != NULL) {
            *nl = '\0';
            print_message(start);
            start = nl + 1;
        }
        // TODO: memmove remaining to front, update buf_len
    }
    return NULL;
}

// input loop
void input_loop(int fd) {
    char line[BUF_SIZE];
    printf("Commands: LOGIN <n> | PLACE <A-J><1-10> <H|V> <len> | READY | FIRE <A-J><1-10> | REMATCH | QUIT\n");
    while (1) {
        printf("> "); fflush(stdout);
        // TODO: fgets() — break on NULL (EOF)
        // TODO: trim_newline, skip empty lines
        // TODO: send_msg(fd, "%s\n", line)
    }
}

// draw grids
void update_grid(char grid[GRID_SIZE][GRID_SIZE], int row, int col, char val) {
    grid[row][col] = val;
}

void print_grids(void) {
    printf("\n  %-26s  %s\n", "Your board", "Opponent board");
    printf("  ");
    for (int c = 0; c < GRID_SIZE; c++) printf("%c ", 'A' + c);
    printf("   ");
    for (int c = 0; c < GRID_SIZE; c++) printf("%c ", 'A' + c);
    printf("\n");
    for (int r = 0; r < GRID_SIZE; r++) {
        printf("%2d", r + 1);
        for (int c = 0; c < GRID_SIZE; c++) {
            // TODO: print own_grid[r][c] with colours (S=green, H=red, M=blue)
        }
        printf("   ");
        for (int c = 0; c < GRID_SIZE; c++) {
            // TODO: print shot_grid[r][c] with colours (H=red, M=blue)
        }
        printf("\n");
    }
    printf("\n> "); fflush(stdout);
}

void print_message(const char *msg) {
    printf("\r");
    char copy[BUF_SIZE];
    strncpy(copy, msg, BUF_SIZE - 1);
    char *cmd  = strtok(copy, " ");
    char *rest = strtok(NULL, "");
    if (!cmd) return;

    if      (!strcmp(cmd, "HIT"))      { update_grid(shot_grid, 0, 0, 'H'); /* TODO: parse coords */ print_grids(); printf(ANSI_RED   "HIT!\n"               ANSI_RESET); }
    else if (!strcmp(cmd, "MISS"))     { update_grid(shot_grid, 0, 0, 'M'); /* TODO: parse coords */ print_grids(); printf(ANSI_BLUE  "Miss.\n"              ANSI_RESET); }
    else if (!strcmp(cmd, "OPP_HIT"))  { update_grid(own_grid,  0, 0, 'H'); /* TODO: parse coords */ print_grids(); printf(ANSI_RED   "Opponent hit!\n"      ANSI_RESET); }
    else if (!strcmp(cmd, "OPP_MISS")) { update_grid(own_grid,  0, 0, 'M'); /* TODO: parse coords */ print_grids(); printf(             "Opponent missed.\n"             ); }
    else if (!strcmp(cmd, "YOUR_TURN"))  printf(ANSI_BOLD "Your turn — fire!\n"        ANSI_RESET);
    else if (!strcmp(cmd, "OPP_TURN"))   printf(          "Waiting for opponent...\n"             );
    else if (!strcmp(cmd, "WIN"))        printf(ANSI_BOLD ANSI_GREEN "You win!\n"      ANSI_RESET);
    else if (!strcmp(cmd, "LOSE"))       printf(ANSI_RED  "You lose.\n"                ANSI_RESET);
    else if (!strcmp(cmd, "SUNK"))       printf(ANSI_BOLD "You sank a ship!\n"         ANSI_RESET);
    else if (!strcmp(cmd, "OPP_SUNK"))   printf(ANSI_RED  "Your ship was sunk!\n"      ANSI_RESET);
    else if (!strcmp(cmd, "PLACING"))  { printf("Place your ships.\n"); print_grids(); }
    else if (!strcmp(cmd, "WAIT"))       printf("Waiting for opponent...\n");
    else if (!strcmp(cmd, "ERR"))        printf(ANSI_RED "Error: %s\n" ANSI_RESET, rest ? rest : "");
    else                                 printf("%s\n", msg);

    printf("> "); fflush(stdout);
}

// main loop
int main(int argc, char *argv[]) {
    if (argc != 3) { fprintf(stderr, "usage: %s <host> <port>\n", argv[0]); exit(1); }
    init_grid(own_grid);
    init_grid(shot_grid);
    sockfd = connect_to_server(argv[1], atoi(argv[2]));
    printf("Connected to %s:%s\n", argv[1], argv[2]);
    pthread_t tid;
    // TODO: pthread_create(&tid, NULL, recv_loop, &sockfd)
    // TODO: pthread_detach(tid)
    input_loop(sockfd);
    close(sockfd);
    return 0;
}