#include "utils.h"
#include "game.h"
#include <errno.h>

// Wire format "A5": letter A-J = column 0-9, digits = row 1-10 -> internal row 0-9, col 0-9 (grid[row][col]).
int parse_coords(const char *str, int *row, int *col) {
    if (!str || strlen(str) < 2) return 0;
    *col = str[0] - 'A';
    *row = atoi(str + 1) - 1;
    return is_valid_cell(*row, *col);
}

void send_msg(int fd, const char *fmt, ...) {
    char buf[BUF_SIZE];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len < 0) return;
    if (len >= (int)sizeof(buf)) len = (int)sizeof(buf) - 1;

    int sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, buf + sent, (size_t)(len - sent), 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return;
        }
        if (n == 0) return;
        sent += (int)n;
    }
}

void trim_newline(char *str) {
    if (!str) return;
    size_t n = strlen(str);
    while (n > 0 && (str[n - 1] == '\n' || str[n - 1] == '\r')) {
        str[n - 1] = '\0';
        n--;
    }
}
