#include "common.h"
#include "utils.h"
#include "game.h"

// Local boards match server/game: '.' water, 'S' ship, 'X' hit ship, 'M' miss. shot_grid: '.' unfired.
#define ANSI_RED   "\033[31m"
#define ANSI_GREEN "\033[32m"
#define ANSI_BLUE  "\033[34m"
#define ANSI_BOLD  "\033[1m"
#define ANSI_RESET "\033[0m"

int  sockfd;
char own_grid[GRID_SIZE][GRID_SIZE];
char shot_grid[GRID_SIZE][GRID_SIZE];

void update_grid(char grid[GRID_SIZE][GRID_SIZE], int row, int col, char val);
void print_grids(void);
void print_message(const char *msg);

// setup
int connect_to_server(const char *host, int port) {
    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%d", port);
    struct addrinfo hints, *res, *p;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, port_str, &hints, &res) != 0) {
        perror("getaddrinfo");
        return -1;
    }

    int fd = -1;
    for (p = res; p != NULL; p = p->ai_next) {
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0) continue;
        if (connect(fd, p->ai_addr, p->ai_addrlen) == 0) break;
        close(fd);
        fd = -1;
    }

    freeaddrinfo(res);
    if (fd < 0) perror("connect");
    return fd;
}

// recv thread
void *recv_loop(void *arg) {
    char buf[BUF_SIZE];
    int  buf_len = 0;
    int  fd = *(int *)arg;
    while (1) {
        int n = recv(fd, buf + buf_len, BUF_SIZE - buf_len - 1, 0);
        if (n <= 0) {
            printf("\nDisconnected from server.\n");
            exit(0);
        }
        buf_len += n;
        buf[buf_len] = '\0';

        char *start = buf, *nl;
        while ((nl = strchr(start, '\n')) != NULL) {
            *nl = '\0';
            print_message(start);
            start = nl + 1;
        }
        int remaining = (int)((buf + buf_len) - start);
        memmove(buf, start, remaining);
        buf_len = remaining;
    }
    return NULL;
}

// input loop
void input_loop(int fd) {
    char line[BUF_SIZE];
    printf("Commands: LOGIN <n> | PLACE <A-J><1-10> <H|V> <len> (fleet 2,3,3,4,5 any order) | READY | FIRE <A-J><1-10> | REMATCH | QUIT\n");
    while (1) {
        printf("> "); fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) break;
        trim_newline(line);
        if (line[0] == '\0') continue;
        send_msg(fd, "%s\n", line);
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
            char cell = own_grid[r][c];
            if      (cell == 'S') printf(ANSI_GREEN "%c " ANSI_RESET, cell);
            else if (cell == 'X') printf(ANSI_RED "%c " ANSI_RESET, cell);
            else if (cell == 'M') printf(ANSI_BLUE "%c " ANSI_RESET, cell);
            else                  printf("%c ", cell);
        }
        printf("   ");
        for (int c = 0; c < GRID_SIZE; c++) {
            char cell = shot_grid[r][c];
            if      (cell == 'X') printf(ANSI_RED "%c " ANSI_RESET, cell);
            else if (cell == 'M') printf(ANSI_BLUE "%c " ANSI_RESET, cell);
            else                  printf("%c ", cell);
        }
        printf("\n");
    }
    printf("\n> "); fflush(stdout);
}

void print_message(const char *msg) {
    printf("\r");
    // One line: "BOARD_OWN " then 100 chars (10 rows of own_grid); server authoritative for '.' 'S' 'X' 'M'.
    if (strncmp(msg, "BOARD_OWN ", 10) == 0) {
        const char *s = msg + 10;
        int n = (int)strlen(s);
        if (n >= GRID_SIZE * GRID_SIZE) {
            for (int r = 0; r < GRID_SIZE; r++) {
                for (int c = 0; c < GRID_SIZE; c++) {
                    own_grid[r][c] = s[r * GRID_SIZE + c];
                }
            }
            print_grids();
        }
        printf("> "); fflush(stdout);
        return;
    }

    char copy[BUF_SIZE];
    strncpy(copy, msg, BUF_SIZE - 1);
    copy[BUF_SIZE - 1] = '\0';
    char *cmd  = strtok(copy, " ");
    char *rest = strtok(NULL, "");
    if (!cmd) return;

    // Server sends "HIT"/"MISS" tokens; we store hits as 'X' on grids (same as game.c).
    if      (!strcmp(cmd, "HIT"))      { int r, c; if (rest && parse_coords(rest, &r, &c)) update_grid(shot_grid, r, c, 'X'); print_grids(); printf(ANSI_RED   "HIT!\n"               ANSI_RESET); }
    else if (!strcmp(cmd, "MISS"))     { int r, c; if (rest && parse_coords(rest, &r, &c)) update_grid(shot_grid, r, c, 'M'); print_grids(); printf(ANSI_BLUE  "Miss.\n"              ANSI_RESET); }
    else if (!strcmp(cmd, "OPP_HIT"))  { int r, c; if (rest && parse_coords(rest, &r, &c)) update_grid(own_grid,  r, c, 'X'); print_grids(); printf(ANSI_RED   "Opponent hit!\n"      ANSI_RESET); }
    else if (!strcmp(cmd, "OPP_MISS")) { int r, c; if (rest && parse_coords(rest, &r, &c)) update_grid(own_grid,  r, c, 'M'); print_grids(); printf(             "Opponent missed.\n"             ); }
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
    if (sockfd < 0) exit(1);
    printf("Connected to %s:%s\n", argv[1], argv[2]);
    pthread_t tid;
    if (pthread_create(&tid, NULL, recv_loop, &sockfd) != 0) {
        perror("pthread_create");
        close(sockfd);
        exit(1);
    }
    pthread_detach(tid);
    input_loop(sockfd);
    close(sockfd);
    return 0;
}