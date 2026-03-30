#ifndef UTILS_H
#define UTILS_H

#include "common.h"

int  parse_coords(const char *str, int *row, int *col); // "A5" -> row=4 col=0
void send_msg    (int fd, const char *fmt, ...);
void trim_newline(char *str);

#endif
