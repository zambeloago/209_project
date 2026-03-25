#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

int setup_server(int port) {
    int listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_socket == -1) {
        //socket error handling
        perror("socket");
        exit(1);
    }
    int opt = 1;
    setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = INADDR_ANY;

    if ( bind(listen_socket, (struct sockaddr*) &server_address, sizeof(server_address)) == -1) {
        perror("bind");
        close(listen_socket);
        exit(1);
        //remember to set up a socket and bind on client code
    }

    if (listen(listen_socket, 2) == -1) {
        perror("listen");
        close(listen_socket);
        exit(1);
    }
    q
    printf("Server listening on port %d\n", port);
    return listen_socket;
}