#include "utils.h"

// "A5" -> row=4, col=0
int parse_coords(const char *str, int *row, int *col) {
    if (!str || strlen(str) < 2) return 0;
    *col = str[0] - 'A';
    *row = atoi(str + 1) - 1;
    return is_valid_cell(*row, *col);
}

void send_msg(int fd, const char *fmt, ...) {
    char buf[BUF_SIZE];
    va_list args;
    // TODO: va_start, vsnprintf into buf, va_end
    // TODO: loop send() until all bytes sent
}

void trim_newline(char *str) {
    // TODO: strip trailing '\n' and '\r'
}
