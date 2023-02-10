#ifndef _NETWORK_SOCKET_H
#define _NETWORK_SOCKET_H

#include <stdio.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

bool network_socket_state = false;

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
   

    printf("Connection accepted!");

    network_socket_state = true;
    while (network_socket_state) {
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (newsockfd < 0)
            error("ERROR on accept");
        
        int capacity = 4096;
        char* full_message = (char*)malloc(capacity*sizeof(char));
        int ptr = 0;
        while(true) {
            bzero(buffer, 256);
            n = read(newsockfd, buffer, 255);
            if (n < 0) error("ERROR reading from socket");
            if (n == 0) break;
            printf("%i %s", n, buffer);
            // Copy into buffer
            for(int i=0; i<n; i++) {
                full_message[ptr++] = buffer[i];
                if(ptr >= capacity) {
                    capacity *= 2;
                    char* new_buffer = (char*)malloc(capacity*sizeof(char));
                    memcpy(new_buffer, full_message, ptr*sizeof(char));
                    free(full_message);
                    full_message = new_buffer;
                }
            }
        }
        printf("Connection terminated! Listening...\n");
        full_message[ptr++] = '\0';
        network_socket_process(full_message, ptr);
        free(full_message);

        close(newsockfd);
    }
}

void network_socket_process(char* buffer, int len) {
    printf("STRING:\n%s\n", buffer);

    printf("RAW:\n");
    char* cp = buffer;
    for ( ; *cp != '\0'; ++cp ) {
        printf("%02X ", *cp);
    }
    fflush(stdout);
}

void network_socket_exit() {
    network_socket_state = false;
}

#endif // !_NETWORK_SOCKET_H