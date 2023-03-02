#ifndef _NETWORK_SOCKET_H
#define _NETWORK_SOCKET_H

#include <stdio.h>
#include <sys/param.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <math.h> // M_PI

#include "decode.h" // avlc_decoder_queue_push
#include "output-common.h" // vdl2_msg_metadata

#include "dumpvdl2.h" // vdl2_channel_t, vdl2_channel_init, process_buf_c64


bool network_socket_state = false;
vdl2_channel_t* socket_channel = NULL;

void network_socket_init() {
    printf("TEST NETWORK SOCKET INIT\n");

    int sockfd, portno, clilen, n;

    struct sockaddr_in serv_addr;

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
        serv_addr.sin_port = htons(++portno);
        printf("Select new port: %d\n", portno);
        if(count-- == 0)
            exit(1);
        goto retry;
    }
    listen(sockfd, 5);
    printf("Listening on port %d...\n", portno);

    socket_channel = vdl2_channel_init(0, 0, 0, 0);
    // test_bitstream_scrambler();
    network_socket_main(sockfd);
}

void network_socket_main(int sockfd) {
    uint8_t buffer[256];

    network_socket_state = true;
    while (network_socket_state) {
        struct sockaddr_in cli_addr;
        int clilen = sizeof(cli_addr);
        int newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        printf("Connection accepted!\n");

        if (newsockfd < 0) {
            printf("ERROR on accept\n");
            exit(1);
        }
        int capacity = 4096;
        uint8_t* full_message = (uint8_t*)malloc(capacity*sizeof(uint8_t));
        int ptr = 0;
        while(true) {
            bzero(buffer, 256);
            int n = read(newsockfd, buffer, 255);
            if (n < 0) { printf("ERROR reading from socket"); exit(1); }
            if (n == 0) break;
            // Copy into buffer
            for(int i=0; i<n; i++) {
                full_message[ptr++] = buffer[i];
                if(ptr >= capacity) {
                    capacity *= 2;
                    uint8_t* new_buffer = (uint8_t*)malloc(capacity*sizeof(uint8_t));
                    memcpy(new_buffer, full_message, ptr*sizeof(uint8_t));
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

#define LFSR_IV 0x6959u
void test_bitstream_scrambler() {
    bitstream_t*  bs = bitstream_init(1024);
    uint8_t data_in[] = { 0x12, 0x34, 0x56, 0xFF, 0x00 };
    char* hxd = hexdump(data_in, sizeof(data_in)); printf("DATA UNSCRAMBLED\n%s\n", hxd); free(hxd);
    
    bitstream_append_lsbfirst(bs, data_in, sizeof(data_in), 8);

    uint16_t lfsr = LFSR_IV;
    printf("BIT\n");
    bitstream_descramble(bs, &lfsr); // Scramble
    printf("\n");
    bitstream_read_lsbfirst(bs, data_in, sizeof(data_in), 8);
    hxd = hexdump(data_in, sizeof(data_in)); printf("DATA SCRAMBLED\n%s\n", hxd); free(hxd);

    bitstream_reset(bs);
    // bs = bitstream_init(1024);

    bitstream_append_lsbfirst(bs, data_in, sizeof(data_in), 8);
    lfsr = LFSR_IV;
    printf("BIT\n");
    bitstream_descramble(bs, &lfsr); // Scramble
    printf("\n");


    bitstream_read_lsbfirst(bs, data_in, sizeof(data_in), 8);
    hxd = hexdump(data_in, sizeof(data_in)); printf("DATA RESCRAMBLED\n%s\n", hxd); free(hxd);


}

void network_socket_process(uint8_t* buf, int len) {
    vdl2_channel_t* v = socket_channel;

    // printf("Data into socket (HEXDUMP):\n");
    // dump_hex(buffer, len, 16);
    // printf("\n");
 

    demod_sync_init();
	// sbuf = XCALLOC(32*16384, sizeof(float));
    // printf("SYNC BUF IDX: %d\n", v->syncbufidx);


    // float* fbuf = (float*)buf;
    // len = len / 4; // float
    int cnt = 0;
    const int OVERSAMPLE = 10;
    // Oversample
    for(int i=0; i<len; i+=2) {
        // printf("[%d -> %d] (%.2f, %.2f) -> %.2f\n", i, v->syncbufidx, fbuf[i], fbuf[i+1], (atan2(fbuf[i+1], fbuf[i]) * 8 / (2 * M_PI)));
        if(++cnt == OVERSAMPLE) {
            cnt = 0;
            float re = ((float)buf[i] - 127.5f)/127.5f;
            float im = ((float)buf[i+1] - 127.5f)/127.5f;
            // float re = ((float)buf[i])/255.f;
            // float im = ((float)buf[i+1])/255.f;
            // float re = fbuf[i], im = fbuf[i+1];
            demod(socket_channel, re, im);
        }
        // demod(socket_channel, fbuf[i], fbuf[i+1]);
    }
    // fflush(stdout);

    // SYNC BUF
    // printf("SYNC BUF SIZE: %d\n", v->syncbufidx);
    // for(int i = 1; i < PREAMBLE_SYMS; i++) {
	//     printf("SYNC (%d): %d\n", (v->syncbufidx + (i + 1) * SPS), (int)(v->syncbuf[(v->syncbufidx + (i + 1) * SPS) % SYNC_BUFLEN] * 8.f / (2 * M_PI)));
    //     // printf("%d ", (int)(v->syncbuf[i] * 8 / (2 * M_PI)));
    // }
    // fflush(stdout);

    // printf("Directly into AVLC decoder!\n");
    // NEW(vdl2_msg_metadata, metadata);
    // memset(metadata, 0, sizeof(metadata));
    
    // uint8_t *copy = XCALLOC(len, sizeof(uint8_t));
	// memcpy(copy, buffer, len);
    // avlc_decoder_queue_push(metadata, octet_string_new(copy, len), NULL);
}

void network_socket_exit() {
    network_socket_state = false;
}

#endif // !_NETWORK_SOCKET_H