.PHONY: clean

all: server client

client: client.c utils.c common.h utils.h
	gcc -Wall  -o client client.c utils.c -lpthread

server: server.c game.c utils.c common.h game.h utils.h
	gcc -Wall -o server server.c game.c utils.c

clean:
	rm -f server client