#ifndef _NETWORK_SOCKET_H
#define _NETWORK_SOCKET_H

#include <stdio.h>
#include <sys/param.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "decode.h" // avlc_decoder_queue_push
#include "output-common.h" // vdl2_msg_metadata

bool network_socket_state = false;

void network_socket_init() {
    printf("TEST NETWORK SOCKET INIT\n");

    int sockfd, newsockfd, portno, clilen, n;
    char buffer[256];

    struct sockaddr_in serv_addr, cli_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0) {
        printf("ERROR opening socket");
        exit(1);
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = 5000;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    int count = 5;
retry:
    if (bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("ERROR on binding\n");
        serv_addr.sin_port = htons(portno++);
        printf("Select new port: %d\n", portno);
        if(count-- == 0)
            exit(1);
        goto retry;
    }
    listen(sockfd, 5);
    printf("Listening on port %d...\n", portno);

    clilen = sizeof(cli_addr);

    network_socket_state = true;
    while (network_socket_state) {
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        printf("Connection accepted!\n");

        if (newsockfd < 0) {
            printf("ERROR on accept\n");
            exit(1);
        }
        int capacity = 4096;
        char* full_message = (char*)malloc(capacity*sizeof(char));
        int ptr = 0;
        while(true) {
            bzero(buffer, 256);
            n = read(newsockfd, buffer, 255);
            if (n < 0) { printf("ERROR reading from socket"); exit(1); }
            if (n == 0) break;
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
        network_socket_process(full_message, ptr-1);
        free(full_message);

        close(newsockfd);
    }
}

void dump_hex(const char* data, size_t size, size_t width) {
    for(int i=0; i < size; ++i) {
        printf("%02X ", (uint8_t)data[i]);
        if((i+1) % width == 0) {
            char* buf = (char*)malloc((width+1)*sizeof(char));
            for(int j=0; j<width; j++)buf[j]=data[i-width+1+j];
            buf[width] = '\0';
            for(int i=0; i<=width; i++) {
                if(buf[i] < ' ' || buf[i] > '~')buf[i] = '.';
            }
            printf("| %s\n", buf);
            free(buf);
        }
    }
    int n = (size % width);
    for(int i=0; i<(width-n); i++)printf("   ");
    char* buf = (char*)malloc((n+1)*sizeof(char));
    for(int i=0; i<=width; i++) {
        if(buf[i] == ' ')buf[i] = '.';
    }
    for(int j=0; j<n; j++)buf[j]=data[size - n + j];
    printf("| %s\n", buf);
}

void network_socket_process(char* buffer, int len) {
    printf("Data into socket (HEXDUMP):\n");
    dump_hex(buffer, len, 16);
    printf("\n");
    printf("Into AVLC decoder!\n");

    NEW(vdl2_msg_metadata, metadata);
    memset(metadata, 0, sizeof(metadata));
    
    uint8_t *copy = XCALLOC(len, sizeof(uint8_t));
	memcpy(copy, buffer, len);
    avlc_decoder_queue_push(metadata, octet_string_new(copy, len), NULL);
}

void network_socket_exit() {
    network_socket_state = false;
}

#endif // !_NETWORK_SOCKET_H