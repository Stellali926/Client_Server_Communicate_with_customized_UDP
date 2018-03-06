//
//  Customized_UDP.c
//  Client_Server_Package_Exchange
//
//  Created by Yuxuan Li on 3/6/18.
//  Copyright © 2018 Yuxuan Li. All rights reserved.
//


#include <stdio.h>

#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#define BUFSIZE 10000

#define PAYLOAD_SIZE 255
#define START_ID 0Xffff
#define END_ID 0xffff

#define DATA_PAC    0xfff1
#define ACK_PAC      0xfff2
#define REJECT_PAC   0xfff3

#define DATA_PAC_SIZE    264
#define ACK_PAC_SIZE     8
#define REJECT_PAC_SIZE  10

#define OUT_OF_SEQUENCE         0xfff4
#define LENGTH_MISMATCH         0xfff5
#define END_OF_PACKET_MISSING   0xfff6
#define DUPLICATE_PACKET        0xfff7
#define ACC_PER_PACKET          0xfff8
#define NO_PAID_PACKET          0xfff9
#define NOT_EXIST_PACKET        0xfffa
#define ACCESS_OK_PACKET        0xfffb

#define NUMBER_SIZE 32
#define VERIFICATION_SIZE 128
#define TECH_DATA_SIZE 3
#define SUBSCRIBER_NUM_SIZE (PAYLOAD_SIZE - TECH_DATA_SIZE)

typedef struct packet {
    int packet_type;
    void *data;
} packet;

typedef struct data_packet {
    short start_id, data, end_id;
    char seg_num, client_id, length;
    char payload[PAYLOAD_SIZE];
} data_packet;

typedef struct ack_packet {
    short start_id, ack, end_id;
    char client_id, seg_num;
} ack_packet;

typedef struct reject_packet {
    short start_id, reject, reject_sub, end_id;
    char client_id, recv_seg_num;
} reject_packet;

typedef struct subscriber_info {
    char tech[TECH_DATA_SIZE];
    char subscriber_no[SUBSCRIBER_NUM_SIZE];
} subscriber_info;

void init_pac (packet *pac, int packet_type, void *data) {
    if (pac) {
        pac->packet_type = packet_type;
        pac->data = data;
    }
}

void init_data_pac (data_packet *pac, short start_id, char client_id, short data, char seg_num, char length, char *payload, short end_id) {
    if (pac) {
        pac->start_id = start_id;
        pac->client_id = client_id;
        pac->data = data;
        pac->seg_num = seg_num;
        pac->length = length;
        memcpy(pac->payload, payload, PAYLOAD_SIZE);
        pac->end_id = end_id;
    }
}

void encode_data_pac(const data_packet *datapac, char **pac_bytes) {
    if (datapac && pac_bytes) {
        //copy start_id, client_id, data, seg_num, length, payload, end_id to the memory in order
        char *d = *pac_bytes;
        
        memcpy(d,&(datapac->start_id), sizeof(short));
        d += sizeof(short);
        
        //        fprintf(stderr, "Testing encode step 1.. the encode is %d \n", (unsigned char)strtol(*pac_bytes, NULL, 16)); // test
        
        memcpy(d, &(datapac->client_id),sizeof(char));
        d += sizeof(char);
        
        memcpy(d, &(datapac->data),sizeof(short));
        d += sizeof(short);
        
        memcpy(d, &(datapac->seg_num),sizeof(char));
        d += sizeof(char);
        
        memcpy(d, &(datapac->length), sizeof(char));
        d += sizeof(char);
        
        memcpy(d, datapac->payload, sizeof(char) * PAYLOAD_SIZE);
        d += (sizeof(char) * PAYLOAD_SIZE);
        
        memcpy(d, &(datapac->end_id),sizeof(short));
        d += sizeof(short);
    }
}

void encode_ack_pac(const ack_packet *ackpac, char **pac_bytes) {
    if (ackpac && pac_bytes) {
        char *a = *pac_bytes;
        //copy start_id, client_id, ACK, rece_seg_num, end_id to the memory in order
        memcpy(a,&(ackpac->start_id), sizeof(short));
        a += sizeof(short);
        
        memcpy(a, &(ackpac->client_id), sizeof(char));
        a += sizeof(char);
        
        memcpy(a, &(ackpac->ack), sizeof(short));
        a += sizeof(short);
        
        memcpy(a, &(ackpac->seg_num), sizeof(char));
        a += sizeof(char);
        
        memcpy(a, &(ackpac->end_id), sizeof(short));
    }
}

void encode_reject_pac(const reject_packet *rejectpac, char **pac_bytes) {
    if (rejectpac && pac_bytes) {
        char *r = *pac_bytes;
        //copy start_id, client_id, reject, reject sub code, received segment number, end_id to the memory in order
        memcpy(r,&(rejectpac->start_id), sizeof(short));
        r += sizeof(short);
        
        memcpy(r, &(rejectpac->client_id), sizeof(char));
        r += sizeof(char);
        
        memcpy(r, &(rejectpac->reject), sizeof(short));
        r += sizeof(short);
        
        memcpy(r, &(rejectpac->reject_sub), sizeof(short));
        r += sizeof(short);
        
        memcpy(r, &(rejectpac->recv_seg_num), sizeof(char));
        r += sizeof(char);
        
        memcpy(r, &(rejectpac->end_id), sizeof(short));
    }
}

void encode_packet(const packet *pac, char **pac_bytes, int *size) {
    if (pac_bytes && size && pac) {
        unsigned short type = (unsigned short)pac->packet_type;
        
        if (type == ACC_PER_PACKET || type == DATA_PAC || type == ACCESS_OK_PACKET || type == NO_PAID_PACKET || type == NOT_EXIST_PACKET){
            *pac_bytes = (char *)calloc(DATA_PAC_SIZE, sizeof(char));
            if (*pac_bytes) {
                encode_data_pac((data_packet *)pac->data, pac_bytes);
            }
            *size = DATA_PAC_SIZE;
        } else if (type == ACK_PAC) {
            *pac_bytes = (char *)calloc(ACK_PAC_SIZE, sizeof(char));
            if (*pac_bytes) {
                encode_ack_pac((ack_packet *)pac->data, pac_bytes);
            }
            *size = ACK_PAC_SIZE;
        } else if (type == REJECT_PAC) {
            *pac_bytes = (char *)calloc(REJECT_PAC_SIZE, sizeof(char));
            if (*pac_bytes) {
                encode_reject_pac((reject_packet *)pac->data, pac_bytes);
            }
            *size = REJECT_PAC_SIZE;
        }
    }
}

void decode_data_pac(char *pac_bytes, packet *pac, short start_id, char client_id, short type) {
    data_packet *p = (data_packet *)malloc(sizeof(data_packet));
    
    if(!p) {
        fprintf(stderr, "failed to allocate memory for data packet");
        return;
    }
    
    p->start_id = start_id;
    p->client_id = client_id;
    p->data = type;
    
    //decode seg_num, length, payload, id in order
    memcpy(&(p->seg_num), pac_bytes, sizeof(char));
    pac_bytes += sizeof(char);
    
    memcpy(&(p->length), pac_bytes, sizeof(char));
    pac_bytes += sizeof(char);
    
    memcpy(&(p->payload), pac_bytes, PAYLOAD_SIZE * sizeof(char));
    pac_bytes += (PAYLOAD_SIZE * sizeof(char));
    
    memcpy(&(p->end_id), pac_bytes, sizeof(short));
    
    pac->packet_type = type;
    pac->data = p;
}

void decode_ack_pac (char *pac_bytes, packet *pac, short start_id, char client_id, short type) {
    ack_packet *p = (ack_packet *)malloc(sizeof(ack_packet));
    p->start_id = start_id;
    p->client_id = client_id;
    p->ack = type;
    
    if(!p) {
        fprintf(stderr, "failed to allocate memory for data packet");
        return;
    }
    
    //decode seg_num, end_id in order
    memcpy(&(p->seg_num), pac_bytes, sizeof(char));
    pac_bytes += sizeof(char);
    
    memcpy(&(p->end_id), pac_bytes, sizeof(short));
    
    pac->packet_type = type;
    pac->data = p;
    
}

void decode_rej_pac (char *pac_bytes, packet *pac, short start_id, char client_id, short type) {
    reject_packet *p = (reject_packet *)malloc(sizeof(reject_packet));
    
    if(!p) {
        fprintf(stderr, "failed to allocate memory for data packet");
        return;
    }
    
    p->start_id = start_id;
    p->client_id = client_id;
    p->reject = type;
    
    //decode reject_subcode, received_seg_num, and end_id in order
    memcpy(&(p->reject_sub), pac_bytes, sizeof(short));
    pac_bytes += sizeof(short);
    
    memcpy(&(p->recv_seg_num), pac_bytes, sizeof(char));
    pac_bytes += sizeof(char);
    
    memcpy(&(p->end_id), pac_bytes, sizeof(short));
    
    pac->packet_type = type;
    pac->data = p;
}

void decode_packet(char *pac_bytes, packet *pac) {
    if (!pac_bytes || !pac) {
        return;
    }
    
    char *a = pac_bytes;
    
    short start_id;
    memcpy(&start_id, a, sizeof(short));
    a += sizeof(short);
    
    char client_id;
    memcpy(&client_id, a, sizeof(char));
    a += sizeof(char);
    
    unsigned short type;
    memcpy(&type, a, sizeof(short));
    a += sizeof(short);
    
    if (!a || !pac) {
        return;
    }
    
    if(type == ACC_PER_PACKET || type == DATA_PAC || type == ACCESS_OK_PACKET || type == NO_PAID_PACKET || type == NOT_EXIST_PACKET) {
        decode_data_pac(a, pac, start_id, client_id, type);
    } else if (type == ACK_PAC) {
        decode_ack_pac(a, pac, start_id, client_id, type);
    } else if (type == REJECT_PAC) {
        decode_rej_pac(a, pac, start_id, client_id, type);
    }
}

int sendto_udp(int socket, const packet *pac, int flags, const struct sockaddr *socket_addr, socklen_t socket_len){
    
    char *pac_bytes = NULL;
    int size;
    
    encode_packet(pac, &pac_bytes, &size);
    
    if(size <= 0 || !pac_bytes) {
        return -1;
    }
    
    long res = sendto(socket, pac_bytes, size, flags, socket_addr, socket_len);
    
    if (pac_bytes) {
        free(pac_bytes);
        pac_bytes = NULL;
    }
    
    return (int)res;
}

int recvfrom_udp(int socket, packet *pac, int flags, struct sockaddr *restrict socket_addr, socklen_t *restrict socket_len) {
    char buf[BUFSIZE];
    memset(buf, 0, BUFSIZE);
    
    long res = recvfrom(socket, buf, BUFSIZE, flags, socket_addr, socket_len);
    
    if (res == -1) {
        return (int)res;
    }
    
    decode_packet(buf, pac);
    
    return (int)res;
}

