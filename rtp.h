#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <string.h>
#include <stdlib.h> 


#ifndef _HEADER
#define _HEADER

extern int conn_setup();
extern int rtp_connect(int socket, struct sockaddr_in connectTo);
extern int rtp_send (int socket, void *message, size_t m_length);
extern int rtp_receive(int socket, void *buffer, size_t b_length);
void rtp_accept(int sock);
extern void rtp_close(int sock);
extern void rtp_terminate(int sock);

enum {DATA=0, LAST_DATA, ACK, SYN, SYNACK, TERM, ACK_CONN};

#define MAX_PAYLOAD (500)

typedef struct _PACKET {
    int seq;
    short type;
    int checksum;
    int payload_length;
    char payload[MAX_PAYLOAD];
} packet;

typedef struct _DATA {
	char *stuff;
	int size;
} data;

#endif
