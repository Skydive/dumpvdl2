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
        full_message[ptr++] = '\0';
        network_socket_process(full_message, ptr-1);

        printf("Connection terminated! Listening...\n");
        free(full_message);
        close(newsockfd);
    }
}

void dump_hex(const char* data, size_t size, size_t width) {
    for(int i=0; i < size; ++i) {
        fprintf(stderr, "%02X ", (uint8_t)data[i]);
        if((i+1) % width == 0) {
            char* buf = (char*)malloc((width+1)*sizeof(char));
            for(int j=0; j<width; j++)buf[j]=data[i-width+1+j];
            buf[width] = '\0';
            for(int i=0; i<=width; i++) {
                if(buf[i] < ' ' || buf[i] > '~')buf[i] = '.';
            }
            fprintf(stderr, "| %s\n", buf);
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
    fprintf(stderr, "| %s\n", buf);
}

void network_socket_process(uint8_t* buf, int len) {
    vdl2_channel_t* v = socket_channel;
    // printf("Data into socket (HEXDUMP):\n");
    // dump_hex(buffer, len, 16);
    // printf("\n");
 

    demod_sync_init();
	// sbuf = XCALLOC(32*16384, sizeof(float));
    // printf("SYNC BUF IDX: %d\n", v->syncbufidx);


    float* fbuf = (float*)buf;
    len = len / 4; // float
    int cnt = 0;
    for(int i=0; i<len; i+=2) {
        // float re = ((float)buf[i]-127.5f)/127.5f;
        // float im = ((float)buf[i+1]-127.5f)/127.5f;
        // printf("%02X %02X ", buf[i], buf[i+1]);
        demod(socket_channel, fbuf[i], fbuf[i+1]);
    }
    // fflush(stdout);

    // SYNC BUF
    // printf("SYNC BUF SIZE: %d\n", v->syncbufidx);
    // for(int i = 1; i < PREAMBLE_SYMS; i++) {
	//     printf("SYNC (%d): %d\n", (v->syncbufidx + (i + 1) * SPS), (int)(v->syncbuf[(v->syncbufidx + (i + 1) * SPS) % SYNC_BUFLEN] * 8.f / (2 * M_PI)));
    //     // printf("%d ", (int)(v->syncbuf[i] * 8 / (2 * M_PI)));
    // }
    // fflush(stdout);

    /*
    printf("Directly into AVLC decoder!\n");
    NEW(vdl2_msg_metadata, metadata);
    memset(metadata, 0, sizeof(metadata));
    
    uint8_t *copy = XCALLOC(len, sizeof(uint8_t));
	memcpy(copy, buf, len);
    avlc_decoder_queue_push(metadata, octet_string_new(copy, len), NULL);
    */
}

void network_socket_exit() {
    network_socket_state = false;
}

// #define LFSR_IV 0x6959u
// void test_bitstream_scrambler() {
//     bitstream_t*  bs = bitstream_init(1024);
//     uint8_t data_in[] = { 0x12, 0x34, 0x56, 0xFF, 0x00 };
//     char* hxd = hexdump(data_in, sizeof(data_in)); printf("DATA UNSCRAMBLED\n%s\n", hxd); free(hxd);
    
//     bitstream_append_lsbfirst(bs, data_in, sizeof(data_in), 8);

//     uint16_t lfsr = LFSR_IV;
//     printf("BIT\n");
//     bitstream_descramble(bs, &lfsr); // Scramble
//     printf("\n");
//     bitstream_read_lsbfirst(bs, data_in, sizeof(data_in), 8);
//     hxd = hexdump(data_in, sizeof(data_in)); printf("DATA SCRAMBLED\n%s\n", hxd); free(hxd);

//     bitstream_reset(bs);
//     // bs = bitstream_init(1024);

//     bitstream_append_lsbfirst(bs, data_in, sizeof(data_in), 8);
//     lfsr = LFSR_IV;
//     printf("BIT\n");
//     bitstream_descramble(bs, &lfsr); // Scramble
//     printf("\n");


//     bitstream_read_lsbfirst(bs, data_in, sizeof(data_in), 8);
//     hxd = hexdump(data_in, sizeof(data_in)); printf("DATA RESCRAMBLED\n%s\n", hxd); free(hxd);
// }


#include "asn1/BIT_STRING.h"
#include "asn1/ACSE-apdu.h"
#include "asn1/CMAircraftMessage.h"
#include "asn1/CMGroundMessage.h"
#include "asn1/Fully-encoded-data.h"
#include "asn1/ProtectedAircraftPDUs.h"
#include "asn1/ProtectedGroundPDUs.h"
#include "asn1/ATCDownlinkMessage.h"
#include "asn1/ATCUplinkMessage.h"
#include "asn1/ADSAircraftPDUs.h"
#include "asn1/ADSGroundPDUs.h"
#include "asn1/ADSMessage.h"
#include "asn1/ADSAccept.h"
#include "asn1/ADSReject.h"
#include "asn1/ADSReport.h"
#include "asn1/ADSNonCompliance.h"
#include "asn1/ADSPositiveAcknowledgement.h"
#include "asn1/ADSRequestContract.h"
#include "dumpvdl2.h"
#include "asn1-util.h"                  // asn1_decode_as(), asn1_pdu_destroy(), asn1_pdu_t, proto_DEF_asn1_pdu
#include "asn1-format-icao.h"           // asn1_*_formatter_table, asn1_*_formatter_table_len
#include "icao.h"

#include "asn1/per_encoder.h"

#include <time.h>


int protect_cpdlc_message() {
    return 0;
}

u_int32_t atn_message_integrity(
    const u_int8_t *data,
    const ssize_t data_len,
    const bool operations_are_a_sequence,
    const bool little_endian
) {
    // State init
    u_int16_t c0 = 0;
    u_int16_t c1 = 0;
    u_int16_t c2 = 0;
    u_int16_t c3 = 0;

    // Process the octets
    for (ssize_t i = 0; i < data_len; i++) {
        // Grab value
        uint8_t octet = data[i];
        // Adding the value of the octet to C0
        c0 = (c0 + octet) % 255;
        // Adding the value of C0 to C1, C1 to C2, and C2 to C3
        if (operations_are_a_sequence) {
            // If we assume that these operation are a sequence (my guess)
            c1 = (c1 + c0) % 255;
            c2 = (c2 + c1) % 255;
            c3 = (c3 + c2) % 255;
        } else {
            // If we assume that these operations are indipendent
            auto old_c1 = c1;
            auto old_c2 = c2;
            c1 = (c1 + c0) % 255;
            c2 = (c2 + old_c1) % 255;
            c3 = (c3 + old_c2) % 255;
        }
    }

    // Grab elements
    u_int16_t x0 = - (c0 + c1 + c2 + c3);
    u_int16_t x1 = c1 + 2 * c2 + 3 * c3;
    u_int16_t x2 = - (c2 + 3 * c3);
    u_int16_t x3 = c3;

    // mod 255
    x0 %= 255;
    x1 %= 255;
    x2 %= 255;
    x3 %= 255;

    // If we assume that it is big endian as with any network proto
    // Technically after mod 255 & 0xFF does nothing, but compilers are smart lmao
    if (little_endian) {
        return ((x3 & 0xFF) << 24) | ((x2 & 0xFF) << 16) | ((x1 & 0xFF) << 8) | (x0 & 0xFF);
    } else {
        return ((x0 & 0xFF) << 24) | ((x1 & 0xFF) << 16) | ((x2 & 0xFF) << 8) | (x3 & 0xFF);
    }
}



uint32_t atn_extended_checksum(uint8_t* buf, int len) {
    dump_hex(buf, len, 16);
    // printf("\n");
    uint16_t c0=0, c1=0, c2=0, c3=0;
    for(int i=0; i<len-4; i++) { // Ignore checksum bytes after protected msg
        uint8_t v = buf[i];
        c0 = (c0 + v) % 255;
        c1 = (c1 + c0) % 255;
        c2 = (c2 + c1) % 255;
        c3 = (c3 + c2) % 255;
        // printf("%02X: c0=%02X c1=%02X c2=%02X c3=%02X\n", v, c0, c1, c2, c3);
    }
    // printf("\n");
    uint16_t x0 = (255 - ((c0 + c1 + c2 + c3) % 255)) % 255;
    uint16_t x1 = (c1 + ((2*c2) % 255) + ((3*c3) % 255)) % 255;
    uint16_t x2 = (255 - (c2 + ((3*c3) % 255) % 255) % 255) % 255;
    uint16_t x3 = c3;
    x0 %= 255; x1 %= 255; x2 %= 255; x3 %= 255;
    // printf("FI: x0=%02X x1=%02X x2=%02X x3=%02X\n", x0, x1, x2, x3);
    // return ((x0 & 0xFF) << 24) | ((x1 & 0xFF) << 16) | ((x2 & 0xFF) << 8) | (x3 & 0xFF);
    return ((x0 & 0xff) << 24) | ((x1 & 0xff) << 16) | ((x2 & 0xff) << 8) | (x3 & 0xff);
}


// int create_cpdlc_downlink_message_freetext(uint8_t *buf, size_t len,
//     uint8_t message_id, time_t rawtime,
//     char* buf_ft) {
//     const static uint8_t template[] = {
//         0x00, 0xa8, 0x0f, 0xa6, 0x60, 0x2f, 0x13, 0x4d, 0x94, 0xd3, 0x5d, 0xe0, 0x62, 0x11, 0x8b, 0x16, 0x2c, 0x58, 0xb1,
//         0x62, 0xc5, 0xa9, 0x16, 0x9d, 0x48, 0xb1, 0x62, 0xc5, 0x8b, 0x14, 0x83, 0xf2, 0xd1, 0x80, 0xb8
//     };

//     // printf("Calling test exported function!\n");
//     Fully_encoded_data_t *fed = NULL;
// 	la_proto_node *node = NULL;
// 	asn_dec_rval_t rval;
// 	rval = uper_decode_complete(0, &asn_DEF_Fully_encoded_data, (void **)&fed, template, sizeof(template));
// 	if(rval.code != RC_OK) {
// 		debug_print(D_PROTO, "uper_decode_complete() failed at position %ld\n", (long)rval.consumed);
// 		return;
// 	}
//     asn_fprint(stderr, &asn_DEF_Fully_encoded_data, fed, 1);

//     // asn_TYPE_descriptor_t *decoded_apdu_type = NULL;
//     // decode_protected_ATCDownlinkMessage((void **)&msg, &decoded_apdu_type, ACSE_apdu_PR_NOTHING, 
//     //     fed->data.presentation_data_values.choice.arbitrary.buf,
//     //     fed->data.presentation_data_values.choice.arbitrary.size);
//     ProtectedAircraftPDUs_t *pairpdu = NULL;
//     asn1_decode_as(&asn_DEF_ProtectedAircraftPDUs, (void **)&pairpdu,
//         fed->data.presentation_data_values.choice.arbitrary.buf,
//         fed->data.presentation_data_values.choice.arbitrary.size);
//     // asn_fprint(stderr, &asn_DEF_ProtectedAircraftPDUs, pairpdu, 1);

//     ATCDownlinkMessage_t *downlink_msg = NULL;
//     ProtectedDownlinkMessage_t *p_downlink_msg = NULL;
//     p_downlink_msg = &pairpdu->choice.send;
//     asn1_decode_as(&asn_DEF_ATCDownlinkMessage, &downlink_msg, p_downlink_msg->protectedMessage->buf, p_downlink_msg->protectedMessage->size);

//     // NEW(ATCDownlinkMessage_t, new_downlink_msg);
//     downlink_msg->header.messageIdNumber = message_id;

//     struct tm *info;
//     info = localtime( &rawtime );

//     downlink_msg->header.dateTime.date.year = info->tm_year+1900;
//     downlink_msg->header.dateTime.date.month = info->tm_mon+1;
//     downlink_msg->header.dateTime.date.day = info->tm_mday;
//     downlink_msg->header.dateTime.timehhmmss.hoursminutes.hours = info->tm_hour;
//     downlink_msg->header.dateTime.timehhmmss.hoursminutes.minutes = info->tm_min;
//     downlink_msg->header.dateTime.timehhmmss.seconds = info->tm_sec;
//     *downlink_msg->header.logicalAck = 0;
    
//     int count = downlink_msg->messageData.elementIds.list.count;
//     int size = downlink_msg->messageData.elementIds.list.size;
//     fprintf(stderr, "Message Data List: %d %d\n", count, size);


//     // const char *octet_str_data = "NUCLEAR ALERT -- PLEASE LAND YOUR PLANE INTO THE LAKE";
//     OCTET_STRING_fromBuf(&downlink_msg->messageData.elementIds.list.array[0]->choice.dM98FreeText,
//         buf_ft, strlen(buf_ft));


//     asn_fprint(stderr, &asn_DEF_ATCDownlinkMessage, downlink_msg, 1);
//     printf("BARRIER!BARRIER!BARRIER!BARRIER!\n %d\n", len);

//     // const char *octet_str_data = "NUCLEAR ALERT -- PLEASE LAND YOUR PLANE INTO THE LAKE";
//     asn_enc_rval_t ret = uper_encode_to_buffer(&asn_DEF_ATCDownlinkMessage, downlink_msg, buf, len);
//     ssize_t bytes = ((ret.encoded + 7) / 8);
//     asn_fprint(stderr, &asn_DEF_ATCDownlinkMessage, buf, 1);
//     return bytes;
//  }

// int wrap_cpdlc_downlink_message(uint8_t *buf, size_t len, 
//     uint8_t *cpdlc_msg_buf, size_t cpdlc_msg_len) {
//     printf("C: Wrap CPDLC Downlink Message!\n");

//     asn_fprint(stderr, &asn_DEF_ATCDownlinkMessage, cpdlc_msg_buf, 1);

//     CPDLCMessage_t cpdlc_message;
//     cpdlc_message.buf = cpdlc_msg_buf;
//     cpdlc_message.size = cpdlc_msg_len;

//     ProtectedAircraftPDUs_t pairpdu;
//     pairpdu.present = ProtectedAircraftPDUs_PR_send;
//     ProtectedDownlinkMessage_t* p_downlink_msg = &pairpdu.choice.send;
//     printf("C: Wrap CPDLC Downlink Message 2!\n");

//     p_downlink_msg->algorithmIdentifier = 0;
//     p_downlink_msg->protectedMessage = &cpdlc_message;
//     printf("C: Wrap CPDLC Downlink Message 3!\n");

//     // TODO: Calculate ATN Extended Checksum...
//     uint8_t integrity_buf[4] = {0x13, 0x37, 0xde, 0xad};
//     p_downlink_msg->integrityCheck.buf = &integrity_buf;
//     p_downlink_msg->integrityCheck.size = sizeof(integrity_buf);


//     printf("C: asn_fprint: pairpdu!\n");

//     asn_fprint(stderr, &asn_DEF_ProtectedAircraftPDUs, &pairpdu, 1);

//     uint8_t pairpdu_buf[100];
//     asn_enc_rval_t ret = uper_encode_to_buffer(&asn_DEF_ProtectedAircraftPDUs, &pairpdu, pairpdu_buf, sizeof(pairpdu_buf));
//     ssize_t bytes = ((ret.encoded + 7) / 8);
//     fprintf(stderr, "pairpdu_buf: Encoded recon: %d, %d\n", ret.encoded, bytes);
//     Fully_encoded_data_t fed;
//     fed.data.presentation_data_values.choice.arbitrary.buf = &pairpdu_buf;
//     fed.data.presentation_data_values.choice.arbitrary.size = bytes;
//     fed.data.presentation_context_identifier = 3;

//     asn_fprint(stderr, &asn_DEF_Fully_encoded_data, &fed, 1);

//     memset(buf, 0, len);
//     ret = uper_encode_to_buffer(&asn_DEF_Fully_encoded_data, &fed, buf, len);
//     bytes = ((ret.encoded + 7) / 8);
//     fprintf(stderr, "BUF:\n");
//     dump_hex(buf, bytes, 16);
//     return bytes;
// }

// ctypes test...
int generate_downlink_freetext(uint8_t *buf, uint32_t len,
    int8_t message_id, time_t rawtime,
    char* buf_ft) {
    const static uint8_t template[] = {
        0x00, 0xa8, 0x0f, 0xa6, 0x60, 0x2f, 0x13, 0x4d, 0x94, 0xd3, 0x5d, 0xe0, 0x62, 0x11, 0x8b, 0x16, 0x2c, 0x58, 0xb1,
        0x62, 0xc5, 0xa9, 0x16, 0x9d, 0x48, 0xb1, 0x62, 0xc5, 0x8b, 0x14, 0x83, 0xf2, 0xd1, 0x80, 0xb8
    };

    // printf("Calling test exported function!\n");
    Fully_encoded_data_t *fed = NULL;
	la_proto_node *node = NULL;
	asn_dec_rval_t rval;
	rval = uper_decode_complete(0, &asn_DEF_Fully_encoded_data, (void **)&fed, template, sizeof(template));
	if(rval.code != RC_OK) {
		debug_print(D_PROTO, "uper_decode_complete() failed at position %ld\n", (long)rval.consumed);
		return;
	}
    asn_fprint(stderr, &asn_DEF_Fully_encoded_data, fed, 1);

    // asn_TYPE_descriptor_t *decoded_apdu_type = NULL;
    // decode_protected_ATCDownlinkMessage((void **)&msg, &decoded_apdu_type, ACSE_apdu_PR_NOTHING, 
    //     fed->data.presentation_data_values.choice.arbitrary.buf,
    //     fed->data.presentation_data_values.choice.arbitrary.size);
    ProtectedAircraftPDUs_t *pairpdu = NULL;
    asn1_decode_as(&asn_DEF_ProtectedAircraftPDUs, (void **)&pairpdu,
        fed->data.presentation_data_values.choice.arbitrary.buf,
        fed->data.presentation_data_values.choice.arbitrary.size);
    asn_fprint(stderr, &asn_DEF_ProtectedAircraftPDUs, pairpdu, 1);

    ATCDownlinkMessage_t *downlink_msg = NULL;
    ProtectedDownlinkMessage_t *p_downlink_msg = NULL;
    p_downlink_msg = &pairpdu->choice.send;
    asn1_decode_as(&asn_DEF_ATCDownlinkMessage, &downlink_msg,
        p_downlink_msg->protectedMessage->buf,
        p_downlink_msg->protectedMessage->size);


    // TODO: Test (what?)
    printf("HASHES:\n");
    printf("Protected Message: %x %x\n",
    p_downlink_msg->protectedMessage->buf[0],
    p_downlink_msg->protectedMessage->buf[p_downlink_msg->protectedMessage->size-1]);
    printf("42 f9 08 68: %x %x %x %x %x\nFC B4 60 2E: %x %x %x %x %x\n", 
        atn_extended_checksum(template, sizeof(template)),
        atn_message_integrity(template, sizeof(template), true, false),
        atn_message_integrity(template, sizeof(template), false, false),
        atn_message_integrity(template, sizeof(template), true, true),
        atn_message_integrity(template, sizeof(template), false, true),
        atn_extended_checksum(p_downlink_msg->protectedMessage->buf, p_downlink_msg->protectedMessage->size),
        atn_message_integrity(p_downlink_msg->protectedMessage->buf, p_downlink_msg->protectedMessage->size, true, false),
        atn_message_integrity(p_downlink_msg->protectedMessage->buf, p_downlink_msg->protectedMessage->size, false, false),
        atn_message_integrity(p_downlink_msg->protectedMessage->buf, p_downlink_msg->protectedMessage->size, true, true),
        atn_message_integrity(p_downlink_msg->protectedMessage->buf, p_downlink_msg->protectedMessage->size, false, true)
    );
    asn_fprint(stderr, &asn_DEF_ATCDownlinkMessage, downlink_msg, 1);

    // NEW(ATCDownlinkMessage_t, new_downlink_msg);
    downlink_msg->header.messageIdNumber = message_id;


    struct tm *info;
    info = localtime( &rawtime );

    downlink_msg->header.dateTime.date.year = info->tm_year+1900;
    downlink_msg->header.dateTime.date.month = info->tm_mon+1;
    downlink_msg->header.dateTime.date.day = info->tm_mday;
    downlink_msg->header.dateTime.timehhmmss.hoursminutes.hours = info->tm_hour;
    downlink_msg->header.dateTime.timehhmmss.hoursminutes.minutes = info->tm_min;
    downlink_msg->header.dateTime.timehhmmss.seconds = info->tm_sec;
    *downlink_msg->header.logicalAck = 0;
    
    int count = downlink_msg->messageData.elementIds.list.count;
    int size = downlink_msg->messageData.elementIds.list.size;
    fprintf(stderr, "Message Data List: %d %d\n", count, size);

    // const char *octet_str_data = "NUCLEAR ALERT -- PLEASE LAND YOUR PLANE INTO THE LAKE";
    OCTET_STRING_fromBuf(&downlink_msg->messageData.elementIds.list.array[0]->choice.dM98FreeText,
        buf_ft, strlen(buf_ft));


    asn_fprint(stderr, &asn_DEF_ATCDownlinkMessage, downlink_msg, 1);

    // Reconstruct


    uint8_t protected_msg_buf[100];
    asn_enc_rval_t ret = uper_encode_to_buffer(&asn_DEF_ATCDownlinkMessage, downlink_msg, protected_msg_buf, sizeof(protected_msg_buf));
    ssize_t bytes = ((ret.encoded + 7) / 8);
    fprintf(stderr, "protected_msg_buf: Encoded recon: %d, %d\n", ret.encoded, bytes);
    p_downlink_msg->protectedMessage->buf = &protected_msg_buf;
    p_downlink_msg->protectedMessage->size = bytes;

    uint8_t integrity_buf[4] = {0x00, 0x00, 0x00, 0x00};
    p_downlink_msg->integrityCheck.buf = &integrity_buf;
    p_downlink_msg->integrityCheck.size = sizeof(integrity_buf);
    asn_fprint(stderr, &asn_DEF_ProtectedAircraftPDUs, pairpdu, 1);



    // p_downlink_msg->protectedMessage->bits_unused = (bytes * 8 - bits);
    
    uint8_t fed_buf[100];
    ret = uper_encode_to_buffer(&asn_DEF_ProtectedAircraftPDUs, pairpdu, fed_buf, sizeof(fed_buf));

    fed->data.presentation_data_values.choice.arbitrary.buf = &fed_buf;
    bytes = ((ret.encoded + 7) / 8);
    fprintf(stderr, "fed_buf: Encoded recon: %d, %d\n", ret.encoded, bytes);
    fed->data.presentation_data_values.choice.arbitrary.size = bytes;
    

    asn_fprint(stderr, &asn_DEF_Fully_encoded_data, fed, 1);


    memset(buf, 0, len);
    ret = uper_encode_to_buffer(&asn_DEF_Fully_encoded_data, fed, buf, len);
    bytes = ((ret.encoded + 7) / 8);
    fprintf(stderr, "BUF:\n");
    dump_hex(buf, bytes, 16);
    return bytes;
}

int generate_uplink_freetext(uint8_t *buf, uint32_t len,
    int8_t message_id, time_t rawtime,
    char* buf_ft) {
    const static uint8_t template[] = {0x00, 0xa8, 0x1a, 0xd3,
        0x60, 0x20, 0x10, 0x2b, 0xc8, 0x46, 0xcd, 0x68, 0x38, 0xe0, 0x2d, 0xca, 0x21, 0xd5, 0xa5, 0x4a, 0x2c,
        0xea, 0x88, 0x20, 0xd4, 0x86, 0x82, 0xac, 0xe9, 0x35, 0x10, 0x4c, 0x8d, 0x42, 0xcd, 0x48, 0xb4, 0xea,
        0x2c, 0x85, 0x4a, 0x2d, 0x49, 0x31, 0xe7, 0x59, 0x59, 0x0e, 0x2c, 0xea, 0x91, 0x69, 0x10, 0x5a, 0x76,
        0x0a, 0x0e, 0x80, 0xa2, 0x88};

    printf("Calling generate_uplink_freetext exported function!\n");
    Fully_encoded_data_t *fed = NULL;
	la_proto_node *node = NULL;
	asn_dec_rval_t rval;
    int x = 0;
	rval = uper_decode_complete(0, &asn_DEF_Fully_encoded_data, (void **)&fed, template+x, sizeof(template)-x);
	if(rval.code != RC_OK) {
		printf("uper_decode_complete() failed at position %ld\n", (long)rval.consumed);
		return;
	}
    asn_fprint(stderr, &asn_DEF_Fully_encoded_data, fed, 1);

    // asn_TYPE_descriptor_t *decoded_apdu_type = NULL;
    // decode_protected_ATCDownlinkMessage((void **)&msg, &decoded_apdu_type, ACSE_apdu_PR_NOTHING, 
    //     fed->data.presentation_data_values.choice.arbitrary.buf,
    //     fed->data.presentation_data_values.choice.arbitrary.size);
    ProtectedGroundPDUs_t *pgndpdu = NULL;
    asn1_decode_as(&asn_DEF_ProtectedGroundPDUs, (void **)&pgndpdu,
        fed->data.presentation_data_values.choice.arbitrary.buf,
        fed->data.presentation_data_values.choice.arbitrary.size);
    asn_fprint(stderr, &asn_DEF_ProtectedGroundPDUs, pgndpdu, 1);

    ATCUplinkMessage_t *uplink_msg = NULL;
    ProtectedUplinkMessage_t *p_uplink_msg = NULL;
    switch(pgndpdu->present) {
		case ProtectedGroundPDUs_PR_startup:
			p_uplink_msg = &pgndpdu->choice.startup;
			break;
		case ProtectedGroundPDUs_PR_send:
			p_uplink_msg = &pgndpdu->choice.send;
			break;
		case ProtectedGroundPDUs_PR_abortUser:
		case ProtectedGroundPDUs_PR_abortProvider:
			printf("Message abort!\n");
			return 0;
		default:
			break;
	}
    asn1_decode_as(&asn_DEF_ATCUplinkMessage, &uplink_msg,
        p_uplink_msg->protectedMessage->buf,
        p_uplink_msg->protectedMessage->size);

    asn_fprint(stderr, &asn_DEF_ATCUplinkMessage, uplink_msg, 1);

    // NEW(ATCDownlinkMessage_t, new_downlink_msg);
    uplink_msg->header.messageIdNumber = message_id;


    struct tm *info;
    info = localtime( &rawtime );

    uplink_msg->header.dateTime.date.year = info->tm_year+1900;
    uplink_msg->header.dateTime.date.month = info->tm_mon+1;
    uplink_msg->header.dateTime.date.day = info->tm_mday;
    uplink_msg->header.dateTime.timehhmmss.hoursminutes.hours = info->tm_hour;
    uplink_msg->header.dateTime.timehhmmss.hoursminutes.minutes = info->tm_min;
    uplink_msg->header.dateTime.timehhmmss.seconds = info->tm_sec;
    *uplink_msg->header.logicalAck = 0;
    
    int count = uplink_msg->messageData.elementIds.list.count;
    int size = uplink_msg->messageData.elementIds.list.size;
    fprintf(stderr, "Message Data List: %d %d\n", count, size);

    // const char *octet_str_data = "NUCLEAR ALERT -- PLEASE LAND YOUR PLANE INTO THE LAKE";
    OCTET_STRING_fromBuf(&uplink_msg->messageData.elementIds.list.array[0]->choice.uM183FreeText,
        buf_ft, strlen(buf_ft));


    asn_fprint(stderr, &asn_DEF_ATCUplinkMessage, uplink_msg, 1);

    // Reconstruct


    uint8_t protected_msg_buf[100];
    asn_enc_rval_t ret = uper_encode_to_buffer(&asn_DEF_ATCUplinkMessage, uplink_msg, protected_msg_buf, sizeof(protected_msg_buf));
    ssize_t bytes = ((ret.encoded + 7) / 8);
    fprintf(stderr, "protected_msg_buf: Encoded recon: %d, %d\n", ret.encoded, bytes);
    p_uplink_msg->protectedMessage->buf = &protected_msg_buf;
    p_uplink_msg->protectedMessage->size = bytes;

    uint8_t integrity_buf[4] = {0x00, 0x00, 0x00, 0x00};
    p_uplink_msg->integrityCheck.buf = &integrity_buf;
    p_uplink_msg->integrityCheck.size = sizeof(integrity_buf);
    asn_fprint(stderr, &asn_DEF_ProtectedGroundPDUs, pgndpdu, 1);



    // p_downlink_msg->protectedMessage->bits_unused = (bytes * 8 - bits);
    
    uint8_t fed_buf[100];
    ret = uper_encode_to_buffer(&asn_DEF_ProtectedGroundPDUs, pgndpdu, fed_buf, sizeof(fed_buf));

    fed->data.presentation_data_values.choice.arbitrary.buf = &fed_buf;
    bytes = ((ret.encoded + 7) / 8);
    fprintf(stderr, "fed_buf: Encoded recon: %d, %d\n", ret.encoded, bytes);
    fed->data.presentation_data_values.choice.arbitrary.size = bytes;
    

    asn_fprint(stderr, &asn_DEF_Fully_encoded_data, fed, 1);


    memset(buf, 0, len);
    ret = uper_encode_to_buffer(&asn_DEF_Fully_encoded_data, fed, buf, len);
    bytes = ((ret.encoded + 7) / 8);
    fprintf(stderr, "BUF:\n");
    dump_hex(buf, bytes, 16);
    return bytes;
}

void double_byte_array(char *buf, int size) {
    printf("RUNNING C CODE!\n");
    for(int i=0; i<size; i++) {
        buf[i] *= 2;
    }
    printf("C CODE DONE!\n");
}

#endif // !_NETWORK_SOCKET_H