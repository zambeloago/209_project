#include <stdio.h>
#include <string.h>
#include "utils.h"

void trim_newline(char *str) {
    if (!str) return;
    size_t len = strlen(str);
    /* Strip \r and/or \n from the end */
    while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r')) {
        str[--len] = '\0';
    }
}

int parse_move(const char *msg, int *row, int *col) {
    if (!msg || !row || !col) return 0;
    /* Expected format: "MOVE <row> <col>" */
    int r, c;
    if (sscanf(msg, "MOVE %d %d", &r, &c) == 2) {
        *row = r;
        *col = c;
        return 1;
    }
    return 0;
}