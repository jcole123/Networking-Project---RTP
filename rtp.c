#include "rtp.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <stdlib.h> 

/*
 * Function prototypes
 */
packet* packetize(void *buffer, int length, int *count);
int checksum(char *payload, int length);
void init_threads(unsigned int n);
void copy(data *buffer, packet *temp);
void rtp_accept(int sock);
int p_receive (int sock, packet *message, int type);
int p_send (int sock, packet *message, int type);
void rtp_terminate(int sock);


/*
 * IP/Port information
 */
struct sockaddr_in connection , connectEst;

/*
 * Timeout
 */
struct timeval timeout;

/*
 * Sequence numbers for send/receive
 */
int sendseq = 0, recseq = 0;

//int seq = 0;

void setPayload(packet *temp) {
    int j = temp->type;
    //sanity check
    if(j > 6)
        return;
    switch(j) {
        case 0: 
            strcpy(temp->payload, "data");
            temp->payload_length = strlen("data");
            break;
        case 1: 
            strcpy(temp->payload, "last_data");
            temp->payload_length = strlen("last_data");
            break;
        case 2: 
            strcpy(temp->payload, "ack");
            temp->payload_length = strlen("ack");
            break;
        case 3: 
            strcpy(temp->payload, "synch");
            temp->payload_length = strlen("synch");
            break;
        case 4: 
            strcpy(temp->payload, "synack");
            temp->payload_length = strlen("synack");
            break;
        case 5: 
            strcpy(temp->payload, "term");
            temp->payload_length = strlen("term");
            break;
        case 6:
            strcpy(temp->payload, "ackcon");
            temp->payload_length = strlen("ackcon");
            break;

    }
    temp->checksum = checksum(temp->payload, temp->payload_length);
}

/*
 * Create UDP socket and allocate window
 * Return -1 if setup fails
 */
int conn_setup() {
	int conn_socket = socket(AF_INET, SOCK_DGRAM, 0);
    //Set timeout
    timeout.tv_sec = 0;
    timeout.tv_usec = 500000;
    return conn_socket;
}

/*
 * Establish connection to server
 * Same as TCP
 */
int rtp_connect(int sock, struct sockaddr_in connectTo) {
    //Already established connection
    if (connection.sin_port == connectTo.sin_port)
        return 1;
    connection = connectTo;
    packet *syn = malloc(sizeof(packet));
    memset(syn, 0, sizeof(packet));

    syn->type = SYN;
    p_send(sock, syn, SYN);
    memset(syn, 0, sizeof(packet));

    syn->type = SYNACK;
    p_send(sock, syn, SYNACK);
    memset(syn, 0, sizeof(packet));

    syn->type = ACK_CONN;
    p_send(sock, syn, ACK_CONN);
    
    connectEst = connection;
    free(syn);
    return 1;
}

void rtp_terminate(int sock) {
    //Make sure there's something to terminate
    if (connection.sin_port <= 0) {
        return;
    }
    packet *end = malloc(sizeof(packet));
    memset(end, 0, sizeof(packet));
    end->type = TERM;
    p_send(sock, end, TERM);
    memset(&connection, 0, sizeof(struct sockaddr_in));


}

void rtp_accept(int sock) {
    //If this function is called, then the SYN packet has already been received
    int toRet = -1;
    packet *temp = malloc(sizeof(packet));
    memset(temp, 0, sizeof(packet));
    int size = sizeof(connection);
    temp->type = SYN;
    
    //SynACK packet
    p_receive(sock, temp, SYNACK);
    //Ack packet
    p_receive(sock, temp, ACK_CONN);
    //Only supports one connection at a time
    free(temp);
    connectEst = connection;
}

/*
 * Used to establish connection, makes sure the packet is the right type
 */
int p_send (int sock, packet *message, int type)  {
    int send = -1, recv = -1;
    int end = 0;
    int count = 0;
    int attempts = 0;
    int size = sizeof(connection);
    setPayload(message);
    packet* toCheck = malloc(sizeof(packet));
    setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
    while(!end) {
        memset(toCheck, 0, sizeof(packet));
        send = sendto(sock, message, sizeof(packet), 0, (struct sockaddr *) &connection, size);
        if(send>0) { //packet was sent
            recv = recvfrom(sock, toCheck, sizeof(packet), 0, (struct sockaddr *) &connection, &size);
            if(recv < 0) {
                attempts++;
                //Server is no longer responding
                if(attempts >30)
                    end = 1;
            } else attempts = 0;
            //Check if packet was acknowledged
            if(recv > 0 && toCheck->type == type && toCheck->checksum == checksum(toCheck->payload, toCheck->payload_length)) { //if it has, increment i and move on to the next packet
                end = 1;
                send = -1;
                recv = -1;
            }
        }
    }
    free(toCheck);

    return 1;      
}

/*
 * Used to establish connection, makes sure the packet is the right type
 */
int p_receive(int sock, packet *buffer, int type) {
    int send = -1;
    int end = 0;
    struct sockaddr_in client;
    packet *toGet, *ack;
    int size = sizeof(connection);
    toGet = malloc(sizeof(packet));
    ack = malloc(sizeof(packet)); //packet to be returned to the sender with ACK or 
    while(!end) { 
        memset(ack, 0, sizeof(packet));
        recvfrom(sock, toGet, sizeof(packet), 0, (struct sockaddr *) &connection, &size);
        if(toGet->type == type && toGet->checksum == checksum(toGet->payload, toGet->payload_length)) {
            end = 1; //last data
            ack->type = type;
            setPayload(ack);
            while(send < 0) {
                send = sendto(sock, ack, sizeof(packet), 0, (struct sockaddr *) &connection, sizeof(connection));
            }
        }
        else if(toGet->type != type && toGet->checksum == checksum(toGet->payload, toGet->payload_length)) {
            ack->type = toGet->type;
            setPayload(ack);
            while(send < 0) {
                send = sendto(sock, ack, sizeof(packet), 0, (struct sockaddr *) &connection, sizeof(connection));
            }
        }
        send = -1;
        
    }
    memcpy(buffer, toGet, sizeof(packet));
    free(toGet);
    free(ack);
    return 1; 

}

/*
 * Send message
 * 	message: data to be sent
 *	m_length: length of data in bytes
 * Returns number of bytes sent, -1 if send fails
 */
int rtp_send (int sock, void *message, size_t m_length) {
    int send = -1, recv = -1;
    int i = 0;
    int end = 0;
    int count = 0;
    int last = (recseq-1) >= 0? recseq-1:15;
    int attempts = 0;
    int size = sizeof(connection);
    packet* toSend = packetize(message, m_length, &count);
    packet* toCheck = malloc(sizeof(packet));
    memset(toCheck, 0, sizeof(packet));
    setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
    if(connection.sin_port == 0) 
        return -1;
    while(!end) {
        memset(toCheck, 0 , sizeof(packet));
        toSend[i].seq = sendseq;
        send = sendto(sock, &toSend[i], sizeof(packet), 0, (struct sockaddr *) &connection, size);
        if(send>0) { //packet was sent
            recv = -1;
            recv = recvfrom(sock, toCheck, sizeof(packet), 0, (struct sockaddr *) &connection, &size);
            //Sent something 30 times in a row but received no response, at this point it's safe to assume the ACKNOWLEDGEMENT was just lost
            if(recv < 0) {
                attempts++;
                if(attempts >= 50)
                    end = 1;
            } 
            else attempts = 0;

            //Check if packet was acknowledged
            if(recv > 0 && toCheck->type == ACK && toCheck->seq == sendseq && toCheck->checksum == checksum(toCheck->payload, toCheck->payload_length)) { //if it has, increment i and move on to the next packet
                if(toSend[i].type==LAST_DATA)
                    end = 1;
                i++;
                if(sendseq < 15)
                    sendseq++;
                else sendseq = 0;
                send = -1;
                recv = -1;
            }
            //Client/server is still waiting for an ACKNOWLEDGEMENT for the last packet you received, appease them
            else if(recv > 0 && (toCheck->type == DATA || toCheck->type == LAST_DATA) && toCheck->checksum == checksum(toCheck->payload, toCheck->payload_length) && toCheck->seq == last) {
                packet *ack = malloc(sizeof(packet));
                memset(ack, 0, sizeof(packet));
                send = -1;
                ack->type = ACK;
                setPayload(ack);
                ack->seq = toCheck->seq;
                while(send < 0)
                    send = sendto(sock, ack, sizeof(packet), 0, (struct sockaddr *) &connection, size);
                free(ack);

            }
            //Tried to send a packet but you don't have a connection to the receiver
            else if(toCheck->type == TERM && toCheck->checksum == checksum("term", strlen("term"))) {
                printf("No connection to server\n");
                free(toSend);
                free(toCheck);
                return -1;
            }
        }
    }
    free(toSend);
    free(toCheck);

    return 1;      
}

/*
 * Receive message
 * 	buffer: Buffer to store receieved message
 *	b_length: size of buffer in bytes
 * Return 1 if successful of bytes received, -1 if receive fails
 */
int rtp_receive(int sock, void *buffer, size_t b_length) {
    int send = -1;
    int end = 0;
    int good = 0;
    int recv = 0;
    //Last packet received
    int last = (recseq-1) >= 0? recseq-1:15;
    struct sockaddr_in client;
    packet *toGet, *ack;
    int size = sizeof(connection);
    toGet = malloc(sizeof(packet)); //holds each received packet
    ack = malloc(sizeof(packet)); //packet to be returned to the sender with ACK 
    data *toRet = malloc(sizeof(data));
    toRet->size = 0;
    toRet->stuff = malloc(b_length);
    //Initialise values
    memset(buffer, 0, b_length);
    memset(toRet, 0, sizeof(data));
    memset(ack, 0, sizeof(packet));
    memset(toGet, 0, sizeof(packet));
    while(!end) { 
        memset(ack, 0, sizeof(packet));
        memset(toGet, 0, sizeof(packet));
        recv = recvfrom(sock, toGet, sizeof(packet), 0, (struct sockaddr *) &connection, &size);
        int temp = checksum(toGet->payload, toGet->payload_length);
        //Sender wants to connect
        if(toGet->type == SYN  && toGet->checksum == checksum("synch", strlen("synch"))) {//&& memcmp(&connection, &connectEst, sizeof(struct sockaddr_in)) != 0 ) {
            ack->type = SYN;
            setPayload(ack);
            while(send < 0) 
                send = sendto(sock, ack, sizeof(packet), 0, (struct sockaddr *) &connection, sizeof(connection));
            printf("Establishing connection...\n");
            rtp_accept(sock);
            free(toGet);
            free(toRet);
            free(ack);
            return 1;
        }
        //Sender wants to terminate the connection
        if(toGet->type == TERM && toGet->checksum == checksum("term", strlen("term"))) {
            ack->type = TERM;
            setPayload(ack);
            sendto(sock, ack, sizeof(packet), 0, (struct sockaddr *) &connection, sizeof(connection));
            memset(&connectEst, 0, sizeof(struct sockaddr_in));
            free(toGet);
            free(toRet);
            free(ack);
            return 1;
        }
        //Check to see if a connection has been established with the sender
        if(recv > 0 && memcmp(&connection, &connectEst, sizeof(struct sockaddr_in) != 0) && !good) {
            ack->type = TERM;
            setPayload(ack);
            send = sendto(sock, ack, sizeof(packet), 0, (struct sockaddr *) &connection, sizeof(connection));
            return -1;
        }
        //Get next expected packet, send ACK
        else if(checksum(toGet->payload,toGet->payload_length) == toGet->checksum && toGet->seq == recseq && toGet->type != ACK && toGet->type != ACK_CONN) {
            if(toGet->type==LAST_DATA) { 
                end = 1; //last data
            }
            ack->type = ACK;
            ack->seq = recseq;
            setPayload(ack);
            last = recseq;
            if(recseq < 15)
                recseq++;
            else recseq = 0;
            send = -1;
            recv = 0;
            while(send < 0) {
                send = sendto(sock, ack, sizeof(packet), 0, (struct sockaddr *) &connection, sizeof(connection));
            }
            copy(toRet, toGet);
            
        }
        //Sender is still waiting for you to ACKNOWLEDGE the last packet you received, appease them 
        else if(checksum(toGet->payload,toGet->payload_length) == toGet->checksum && toGet->seq == last) {
            ack->type = ACK;
            ack->seq = toGet->seq;
            setPayload(ack);
            send = -1;
            while(send < 0) {
                send = sendto(sock, ack, sizeof(packet), 0, (struct sockaddr *) &connection, sizeof(connection));
            }
        }
        //Sender never received last packet ACK_CONN when trying to initiate a connection, send it to them 
        else if(toGet->type == ACK_CONN && toGet->checksum == checksum("ackcon", strlen("ackcon"))) {
            ack->type = ACK_CONN;
            setPayload(ack);
            send = -1;
            while(send < 0)
                send = sendto(sock, ack, sizeof(packet), 0, (struct sockaddr *) &connection, sizeof(connection));
        }
        //Let the sender know you're still waiting for a packet, used to make sure the senders "attempts" variable doesn't unintentionally reach 30
        else if (recv > 0) {
            packet *alive = malloc(sizeof(packet));
            alive->seq = -100;
            alive->type = -10;
            alive->checksum = -50;
            sendto(sock,alive,sizeof(packet), 0, (struct sockaddr *) &connection, sizeof(connection));
            free(alive);
        }
        //Used just to avoid comparing connection with connectest when receiving a large number of bytes    
        if(!good) {
            good = 1;
        }
        send = -1;
    }
    //Copy b_length bytes to buffer
    buffer = memcpy(buffer, toRet->stuff, b_length);
    free(toGet);
    free(toRet->stuff);
    free(toRet);
    free(ack);
    return 1; 

}

/*
 * Compute checksum and add it to packet header
 */
int checksum(char *payload, int length) {
    //Corrupt packet, probably unecessary but it makes me feel better
    if(length > MAX_PAYLOAD || length<=0) {
        return rand() % 1000;
    }
    int a = 1;
    int b = 0;
    int i;
    for(i = 0; i < length; i++) {
        a = (a + payload[i]) % 65521;
        b = (b + a) % 65521;
    }
    return (b << 16) | a; 
}

/*
 * Put message into packets
 * 	buffer: message to be sent
 * 	length: size of the message in bytes
 *	count: number of packets created
 */
packet* packetize(void *buff, int length, int *count) {
    //Determine the number of packets needed
    char *buffer = (char*)buff;
    int numPackets = (length/MAX_PAYLOAD);
    if(length % MAX_PAYLOAD !=0) {
        *count = numPackets+1;
        numPackets++;
    }
    else *count = numPackets;
    packet *toRet = malloc(numPackets*sizeof(packet));
    memset(toRet, 0, numPackets*sizeof(packet));
    int j = 0;
    int i, k;
    for(i = 0; i < length; i++) { //fill packet payloads with buffer contents
        if( i > 0 && (i % MAX_PAYLOAD==0))
            j++;
        toRet[j].payload[i - (j*MAX_PAYLOAD)] = buffer[i];
        toRet[j].payload_length++;
    }
    for(k = 0; k < numPackets; k++) { //set data type and checksum
        if(k == (numPackets-1))
            toRet[k].type = LAST_DATA;  
        else toRet[k].type = DATA;
        toRet[k].checksum = checksum(toRet[k].payload, toRet[k].payload_length);
    }
    return toRet;
}

/*
 * Close the socket
 */
void rtp_close(int sock) {
    close(sock);
}

/*
 * Copy packet contents into buffer
 */
void copy(data *buffer, packet *temp) {
    buffer->stuff = (char*)realloc(buffer->stuff, buffer->size + temp->payload_length + 1);
    const int i = buffer->size;
    int j;
    for(j = 0; j<temp->payload_length; j++) {
        buffer->stuff[j+i] = temp->payload[j];
        buffer->size++;
    }
    buffer->stuff[buffer->size] = '\0';
}	

    




