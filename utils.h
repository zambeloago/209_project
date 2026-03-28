#ifndef UTILS_H
#define UTILS_H

/*
 * Parse a "MOVE <row> <col>" message.
 * Writes the row/col into *row and *col.
 * Returns 1 on success, 0 on failure.
 */
int parse_move(const char *msg, int *row, int *col);

/*
 * Remove a trailing '\n' (or '\r\n') from str in-place.
 */
void trim_newline(char *str);

#endif