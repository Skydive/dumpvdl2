#ifndef _NETWORK_SOCKET_H
#define _NETWORK_SOCKET_H

#include <stdio.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

bool network_socket = false;

void network_socket_init() {
    printf("TEST NETWORK SOCKET INIT\n");

    int sockfd, newsockfd, portno, clilen, n;
    char buffer[256];

    struct sockaddr_in serv_addr, cli_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0)
        error("ERROR opening socket");

    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = 5000;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR on binding");
    listen(sockfd, 5);
    printf("Listening on port %d...\n", portno);

    clilen = sizeof(cli_addr);
    newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
    if (newsockfd < 0)
        error("ERROR on accept");

    printf("Connection accepted!");

    socket_state = true;

    while (socket_state) {
        bzero(buffer,256);
        n = read(newsockfd, buffer, 255);
        if (n < 0) error("ERROR reading from socket");
        printf("%s\n",buffer);
    }
}

void network_socket_exit() {
    socket_state = false;
}

#endif // !_NETWORK_SOCKET_H