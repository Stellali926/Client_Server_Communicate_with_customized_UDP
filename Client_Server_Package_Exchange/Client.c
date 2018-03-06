//
//  Client.c
//  Client_Server_Package_Exchange
//
//  Created by Yuxuan Li on 3/6/18.
//  Copyright © 2018 Yuxuan Li. All rights reserved.
//



#include<stdio.h> //printf
#include<string.h> //memset
#include<stdlib.h> //exit(0);
#include <unistd.h>
#include <errno.h>

#include<arpa/inet.h>
#include <sys/types.h>
#include<sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "Customized_UDP.c"

#define CLIENT_ID 0x01
#define CLIENT_ID_NO1 0x01
#define CLIENT_ID_NO2 0x02

#define SERVER "127.0.0.1"
#define BUFLEN 10000  //Max length of buffer
#define PORT 8888   //The port on which to send data

void die(char *s)
{
    perror(s);
    exit(1);
}

int send_for_ack(int sock, struct sockaddr_in *server_addr, packet *pac, int timeout, int retry_times) {
    int i = 0, count = 0;
    unsigned int server_len = sizeof(struct sockaddr_in);
    struct timeval tv;
    tv.tv_sec = timeout;
    tv.tv_usec = 0;
    
    //set timer
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        die("Timeout");
    }
    
    while (i < retry_times) {
        count = (int)sendto_udp(sock, pac, 0, (struct sockaddr *)server_addr, server_len);
        
        if (count < 0) {
            fprintf(stderr, "Cannot send the packet\n");
            return -1;
        }
        
        packet recv_pac;
        recv_pac.data = NULL;
        //retry for ack
        count = recvfrom_udp(sock, &recv_pac, 0, (struct sockaddr *)server_addr, &server_len);
        
        if (count <= 0) { //timeout
            if (errno == EAGAIN) {
                if (i == retry_times - 1) {
                    fprintf(stderr, "Cannot receive ack from server within %d retry times\n", i + 1);
                    fprintf(stderr, "Server does not response\n");
                } else {
                    fprintf(stderr, "Cannot receive ack from server within %d seconds, resend the packet with retry times %d\n", timeout, i + 1);
                }
            } else {
                perror("Cannot get response from server\n");
                return count;
            }
        } else {
            unsigned short pac_type = (unsigned short)recv_pac.packet_type;
            char tech[3] = {0};
            char number[13] = {0};
            
            if (pac_type == ACK_PAC) {
                ack_packet *ack_pac = (ack_packet *)(recv_pac.data);
                printf("Ack from server to client id %02x sequence number %d\n", (unsigned short)ack_pac->client_id, ack_pac->seg_num);
                
            } else if (pac_type == REJECT_PAC) {
                reject_packet *rej_pac = (reject_packet *)(recv_pac.data);
                printf("Reject from server to client id %02x, reject subcode %02x sequence number %d\n", (unsigned short)rej_pac->client_id, (unsigned short)rej_pac->reject_sub, rej_pac->recv_seg_num);
                
            } else if (pac_type == NO_PAID_PACKET) {
                data_packet *d = (data_packet *)(recv_pac.data);
                memcpy(tech, d->payload, 2);
                memcpy(number, d->payload + strlen(tech), 12);
                printf("Get Not Paid Message with the number %s, and using technology %sG\n", number, tech + 1);
                
            } else if (pac_type == NOT_EXIST_PACKET) {
                data_packet *d = (data_packet *)(recv_pac.data);
                memcpy(tech, d->payload, 2);
                memcpy(number, d->payload + strlen(tech), 12);
                printf("Get Not Exist Meesage with the number %s, and using technology %sG\n", number, tech + 1);
            } else if (pac_type == ACCESS_OK_PACKET) {
                data_packet *d = (data_packet *)(recv_pac.data);
                memcpy(tech, d->payload, 2);
                memcpy(number, d->payload + strlen(tech), 12);
                printf("Get Access OK Message with the number %s, and using technology %sG\n", number, tech + 1);
                
            } else{
                printf("Unknown response from server\n");
            }
            
            return count;
        }
        
        if (recv_pac.data) {
            free(recv_pac.data);
        }
        
        i++;
    }
    
    return count;
}

void printFormat() {
    printf("==========================================================================\n");
}


void sned_partial_successful_requests(int sock, struct sockaddr_in *server_addr) {
    if (!server_addr) {
        die("Cannot find server address");
    }
    
    printFormat();
    
    //1. send a successful packet
    printf("\nSend a successful packet with client id %02x and sequence number 1\n", CLIENT_ID_NO2);
    char *p = (char *)calloc(PAYLOAD_SIZE, sizeof(char));
    
    data_packet data_pac;
    packet packet;
    
    init_data_pac(&data_pac, START_ID, CLIENT_ID_NO2, DATA_PAC, 1, strlen(p), p, END_ID);
    init_pac(&packet, DATA_PAC, &data_pac);
    free(p);
    send_for_ack(sock, server_addr, &packet, 3, 3);
    
    //2. error packet with missing sequence number
    printf("\nError with error code %04x\nThe sended packet with client id %02x and sequence number 9 is not in right sequence, the correct sequence is 2.\n", OUT_OF_SEQUENCE, CLIENT_ID_NO2);
    p = (char *)calloc(PAYLOAD_SIZE, sizeof(char));
    
    
    init_data_pac(&data_pac, START_ID, CLIENT_ID_NO2, DATA_PAC, 9, strlen(p), p, END_ID);
    init_pac(&packet, DATA_PAC, &data_pac);
    free(p);
    send_for_ack(sock, server_addr, &packet, 3, 3);
    
    //3. error packet with length mismatch
    printf("\nError with error code %4x\nThe sended packet with client id %02x and sequence number 2 has mismatched length\n", LENGTH_MISMATCH, CLIENT_ID_NO2);
    
    p = (char *)calloc(PAYLOAD_SIZE, sizeof(char));
    
    init_data_pac(&data_pac, START_ID, CLIENT_ID_NO2, DATA_PAC, 2, strlen(p) - 2, p, END_ID);
    init_pac(&packet, DATA_PAC, &data_pac);
    free(p);
    send_for_ack(sock, server_addr, &packet, 3, 3);
    
    //4. error packet with wrong end id
    printf("\nError with error code %04x\nThe sended packet with cliend id %02x and sequence number 3 has wrong end id.\n", END_OF_PACKET_MISSING, CLIENT_ID_NO2);
    p = (char *)calloc(PAYLOAD_SIZE, sizeof(char));
    
    init_data_pac(&data_pac, START_ID, CLIENT_ID_NO2, DATA_PAC, 3, strlen(p), p, 0xfff1);
    init_pac(&packet, DATA_PAC, &data_pac);
    free(p);
    send_for_ack(sock, server_addr, &packet, 3, 3);
    
    //5. error packet with duplicate sequence
    printf("\nError with error code %04x\nThe sended packet with client id %02x and sequence number 2, the sequence number is duplicated.\n", DUPLICATE_PACKET, CLIENT_ID_NO2);
    p = (char *)calloc(PAYLOAD_SIZE, sizeof(char));
    
    init_data_pac(&data_pac, START_ID, CLIENT_ID_NO2, DATA_PAC, 2, strlen(p), p, END_ID);
    init_pac(&packet, DATA_PAC, &data_pac);
    free(p);
    send_for_ack(sock, server_addr, &packet, 3, 3);
    
    printFormat();
}


// request the subscriber which is no paid
void send_not_paid(int sock, struct sockaddr_in *server_addr) {
    if(!server_addr) {
        die("Cannot find server address");
    }
    
    printf("\nSend access request with the subscriber not paid.\n");
    char *p = (char *)calloc(PAYLOAD_SIZE, sizeof(char));
    if (!p) {
        die("Cannot allocate memory to payload");
    }
    
    sprintf(p, "034086668821");
    data_packet data_packet;
    packet pac;
    
    init_data_pac(&data_packet, START_ID, CLIENT_ID, ACC_PER_PACKET, 1, strlen(p), p, END_ID);
    init_pac(&pac, ACC_PER_PACKET, &data_packet);
    free(p);
    send_for_ack(sock, server_addr, &pac, 3, 3);
}

//send access request which the subscriber not found the number
void send_num_not_found(int sock, struct sockaddr_in *server_addr) {
    if(!server_addr) {
        die("Cannot find server address");
    }
    
    printf("\nSend access request which the subscriber does not find the number.\n");
    char *p = (char *)calloc(PAYLOAD_SIZE, sizeof(char));
    if (!p) {
        die("Cannot allocate memory to payload");
    }
    
    sprintf(p, "034444444444");
    data_packet data_packet;
    packet pac;
    
    init_data_pac(&data_packet, START_ID, CLIENT_ID, ACC_PER_PACKET, 2, strlen(p), p, END_ID);
    init_pac(&pac, ACC_PER_PACKET, &data_packet);
    free(p);
    send_for_ack(sock, server_addr, &pac, 3, 3);
}

//send access request which the subscriber not find the message since the technology does not match
void send_tech_not_found(int sock, struct sockaddr_in *server_addr) {
    if(!server_addr) {
        die("Cannot find server address");
    }
    
    printf("\nSend access request which the subscriber does not match.\n");
    char *p = (char *)calloc(PAYLOAD_SIZE, sizeof(char));
    if (!p) {
        die("Cannot allocate memory to payload");
    }
    
    sprintf(p, "014086668821");
    data_packet data_packet;
    packet pac;
    
    init_data_pac(&data_packet, START_ID, CLIENT_ID, ACC_PER_PACKET, 3, strlen(p), p, END_ID);
    init_pac(&pac, ACC_PER_PACKET, &data_packet);
    free(p);
    send_for_ack(sock, server_addr, &pac, 3, 3);
}

//send access request which the subscriber give the permission message
void send_access_ok(int sock, struct sockaddr_in *server_addr) {
    if(!server_addr) {
        die("Cannot find server address");
    }
    
    printf("\nSuccessfully send a request packet with the Access Ok message.\n");
    char *p = (char *)calloc(PAYLOAD_SIZE, sizeof(char));
    if (!p) {
        die("Cannot allocate memory to payload");
    }
    
    sprintf(p, "024086808821");
    data_packet data_packet;
    packet pac;
    
    init_data_pac(&data_packet, START_ID, CLIENT_ID, ACC_PER_PACKET, 4, strlen(p), p, END_ID);
    init_pac(&pac, ACC_PER_PACKET, &data_packet);
    free(p);
    send_for_ack(sock, server_addr, &pac, 3, 3);
}

void run_client() {
    struct sockaddr_in server_addr;
    int sock;
    struct hostent *server;
    
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1)
    {
        die("Cannot open socket");
    }
    
    server = gethostbyname(SERVER);
    if (server == NULL) {
        fprintf(stderr, "Cannot find the host %s\n", SERVER);
        exit(0);
    }
    
    //set the server address
    memset((char *) &server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&((server_addr.sin_addr).s_addr), server->h_length);
    server_addr.sin_port = htons(PORT);
    
    printFormat();
    
    //send partial successful request
    sned_partial_successful_requests(sock, &server_addr);
    
    //send not paid message
    send_not_paid(sock, &server_addr);
    
    //send number not found
    send_num_not_found(sock, &server_addr);
    
    //send technology not fond
    send_tech_not_found(sock, &server_addr);
    
    //access permitted access message
    send_access_ok(sock, &server_addr);
    
    printFormat();
}

int main(int argc, char **argv) {
    run_client();
    return 0;
}

