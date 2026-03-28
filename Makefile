CC     = gcc
CFLAGS = -Wall -Wextra

PORT   ?= 4242

all: server client

server: server.c game.c utils.c game.h utils.h common.h protocol.h
	$(CC) $(CFLAGS) -DPORT=$(PORT) -o server server.c game.c utils.c

client: client.c utils.c utils.h common.h protocol.h
	$(CC) $(CFLAGS) -DPORT=$(PORT) -o client client.c utils.c

clean:
	rm -f server client

.PHONY: all clean