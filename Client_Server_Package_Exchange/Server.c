//
//  Server.c
//  Client_Server_Package_Exchange
//
//  Created by Yuxuan Li on 3/6/18.
//  Copyright © 2018 Yuxuan Li. All rights reserved.
//

#include<stdio.h> //printf
#include<string.h> //memset
#include<stdlib.h> //exit(0);
#include<unistd.h>

#include<netdb.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<sys/types.h>
#include<sys/socket.h>

#include "Customized_UDP.c"

#define BUFLEN 10000  //Max length of buffer
#define PORT 8888   //The port on which to listen for incoming data
#define CLIENT_SIZE 16

#define FILE_PATH ("./Verification_Database.txt")

#define CLIENT_ID 0x01

static int GLOBAL_SEG_NUM = 1;

void die(char *s){
    perror(s);
    exit(1);
}

typedef struct client_info {
    int pac_num;
    char client_id;
} client_info;

typedef struct veri_entry {
    char numbers[NUMBER_SIZE];
    char tech[TECH_DATA_SIZE];
    int paid;
}veri_entry;

void add_seg_num(client_info *client, char client_id, int seg_num) {
    while (client && client->client_id) {
        if (client->client_id == client_id) {
            client->pac_num = seg_num;
            return;
        }
        client++;
    }
    
    if (client && (client->client_id == 0)) {
        client->client_id = client_id;
        client->pac_num = seg_num;
    }
}

void init_subscriber_pacd(data_packet *send, data_packet *recv, veri_entry *entries, int num_data) {
    if (!send || !recv || !entries) {
        return;
    }
    
    int nums = 0, techs = 0;
    
    veri_entry *entry = entries;
    char recv_tech[TECH_DATA_SIZE] = {0};
    char recv_num[NUMBER_SIZE] = {0};
    
    send->start_id = START_ID;
    send->client_id = CLIENT_ID;
    send->end_id = END_ID;
    
    memcpy(recv_tech, recv->payload, (TECH_DATA_SIZE - 1) * sizeof(char));
    memcpy(recv_num, recv->payload + strlen(recv_tech), (NUMBER_SIZE - 1) * sizeof(char));
    
    //check if the number is in the database and the technology matches
    for (int i = 0; i < num_data; i++) {
        if (!strcmp(entry->numbers, recv_num) && !strcmp(entry->tech, recv_tech)) {
            nums = 1;
            techs = 1;
            break;
        } else if (!strcmp(entry->numbers, recv_num)) {
            nums = 1;
        } else if (!strcmp(entry->tech, recv_tech)) {
            techs = 1;
        }
        
        entry++;
    }
    
    if (nums && techs) {
        if (entry->paid) {
            send->data = ACCESS_OK_PACKET;
        } else {
            send->data = NO_PAID_PACKET;
        }
    } else if (nums) {
        send->data = NOT_EXIST_PACKET;
    } else {
        send->data = NOT_EXIST_PACKET;
    }
    
    strcat(send->payload, recv_tech);
    strcat(send->payload + strlen(recv_tech), recv_num);
    send->seg_num = GLOBAL_SEG_NUM++;
}

void init_sendpac(packet *recv_pac, packet *send_pac, client_info *clients, veri_entry *entries, int num_data) {
    if (recv_pac && send_pac && clients) {
        
        data_packet *d = (data_packet *)(recv_pac->data);
        
        short type = ACK_PAC;
        char client_id = d->client_id;
        char seg_num = d->seg_num;
        short reject_sub = 0;
        
        //check sequence
        int cur_num = -1;
        while (clients && clients->client_id) {
            if (clients->client_id == client_id) {
                cur_num = clients->pac_num;
                break;
            }
            
            clients++;
        }
        
        if (cur_num == -1) {
            if (seg_num != 1) {
                type = REJECT_PAC;
                reject_sub = OUT_OF_SEQUENCE;
            } else {
                add_seg_num(clients, client_id, 1);
            }
        } else {
            if (cur_num + 1 == seg_num) {
                add_seg_num(clients, client_id, seg_num);
            } else if (seg_num > cur_num + 1) {
                type = REJECT_PAC;
                reject_sub = OUT_OF_SEQUENCE;
            } else {
                type = REJECT_PAC;
                reject_sub = DUPLICATE_PACKET;
            }
        }
        
        //check length
        if (d->length != strlen(d->payload)) {
            type = REJECT_PAC;
            reject_sub = LENGTH_MISMATCH;
        }
        
        //check id
        if ((unsigned short)(d->end_id) != END_ID) {
            type = REJECT_PAC;
            reject_sub = END_OF_PACKET_MISSING;
        }
        
        //if no error before, check which acces message to send back
        if ((unsigned short)type == ACK_PAC) {
            data_packet *dsend = (data_packet *)malloc(sizeof(data_packet));
            
            if(!dsend) {
                die("Cannot allocate memory for ack packet");
            }
            init_subscriber_pacd(dsend, d, entries, num_data);
            
            send_pac->packet_type = dsend->data;
            send_pac->data = dsend;
        } else {
            reject_packet *r = (reject_packet *)malloc(sizeof(reject_packet));
            if (!r) {
                die("Cannot allocate memory for reject packet");
            }
            
            r->start_id = START_ID;
            r->client_id = client_id;
            r->reject = REJECT_PAC;
            r->reject_sub = reject_sub;
            r->recv_seg_num = seg_num;
            r->end_id = END_ID;
            
            send_pac->packet_type = REJECT_PAC;
            send_pac->data = r;
        }
    }
}

void run_server(veri_entry *entries, int num_data) {
    struct sockaddr_in server_addr, client_addr;
    struct hostent *hostp;
    client_info clients[CLIENT_SIZE];
    packet recv_pac;
    
    int sock, opt, count;
    char *host_addr;
    unsigned int socketlen = sizeof(client_addr);
    
    bzero(clients, CLIENT_SIZE * sizeof(client_info));
    
    //create a UDP socket
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1)
    {
        die("Cannot open socket");
    }
    
    opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const void *)&opt, sizeof(int));
    
    // zero out the structure
    memset((char *) &server_addr, 0, sizeof(server_addr));
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((unsigned short)PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    //bind socket to port
    if( bind(sock , (struct sockaddr*)&server_addr, sizeof(server_addr) ) == -1)
    {
        die("Cannot bind to port");
    }
    
    //keep listening for data
    while(1) {
        //try to receive some data, this is a blocking call
        count = recvfrom_udp(sock, &recv_pac, 0, (struct sockaddr *) &client_addr, &socketlen);
        
        if (count == -1) {
            die("Cannot receive from client");
        }
        
        hostp = gethostbyaddr((const char *)&client_addr.sin_addr.s_addr, sizeof(client_addr.sin_addr.s_addr), AF_INET);
        if (hostp == NULL) {
            die ("Cannot get the host by address");
        }
        
        packet send_pac;
        send_pac.data = 0;
        
        //generate_send_packet(&recv_packet, &send_packet, client_infos);
        init_sendpac(&recv_pac, &send_pac, clients, entries, num_data);
        
        fprintf(stderr, "server received data from %s\npacket type: %02x\n\n", hostp->h_name, (unsigned short)recv_pac.packet_type);
        
        //reply the client with the same data
        count = sendto_udp(sock, &send_pac, 0, (struct sockaddr *)&client_addr, socketlen);
        
        if (count == -1) {
            die("Cannot send data back to clients");
        }
        
        if (send_pac.data) {
            free(send_pac.data);
        }
    }
}

//read data from file and stored in veri_entry
int read_from_files(veri_entry *r) {
    FILE *file = fopen(FILE_PATH, "r");
    
    int n = 0;
    while (fscanf(file, "%s %s %d\n", r->numbers, r->tech, &(r->paid)) == 3 && n < VERIFICATION_SIZE) {
        int i = 0;
        int j = 0;
        char *number = r->numbers;
        while (j < strlen(number)) {
            if(*(number + j) != '-') {
                *(number + i) = *(number + j);
                i++;
            }
            j++;
        }
        
        *(number + i) = 0;
        r++;
        n++;
    }
    
    fclose(file);
    return n;
}

int main(int argc, char **argv) {
    veri_entry veri_entries[VERIFICATION_SIZE];
    bzero(veri_entries, VERIFICATION_SIZE * sizeof(veri_entry));
    
    //read file
    int num_data = read_from_files(veri_entries);
    
    run_server(veri_entries, num_data);
}



