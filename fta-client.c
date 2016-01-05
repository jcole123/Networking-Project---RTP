#include "rtp.h"
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <string.h>
#include <stdlib.h> 



void sendFile(char *buff);
void recvFile(char *buff);

#define MAX_INPUT 20

char userInput[200], *udpPort, *ip, *emuPort, *buffer;

struct sockaddr_in server, client;

FILE *fp;

int sent;

int sock;

int main(int argc, char *argv[]) {
	if(argc < 4 || argc > 4) {
		printf("Invalid number of parameters");
		exit(0);
	}
	sock = conn_setup();
    int tobind = atoi(argv[1]);
    client.sin_family = AF_INET;
    client.sin_addr.s_addr = INADDR_ANY;
    client.sin_port = htons(tobind);
   
    if (bind(sock, (struct sockaddr *) &client, sizeof(client)) < 0) {
        printf("Unable to bind socket, aborting... \n");
        exit(0);
    }

    int port = atoi(argv[3]);
    memset(&server, 0, sizeof(server)); 
    server.sin_addr.s_addr = inet_addr(argv[2]);
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
	sock -1;
    if(sock < 0) {
        printf("Failed to create socket\n");
        exit(-1);
    }

	while(1) {
        memset(userInput, 0, 200);
		//Get user input and remove newline char
		fgets(userInput, 200, stdin);
		userInput[strlen(userInput) -1] = '\0';
		strtok(userInput, " ");
		char *filename = strtok(NULL, " ");
        if(filename == NULL && (userInput[0] != 'c' && userInput[0] != 'd')) {
        	printf("No file name specified\n");
        	userInput[0] = '\0';
        }
		if(!strcmp(userInput, "connect")) {
			if(rtp_connect(sock, server) < 1) {
				printf("Could not establish connection\n");
				exit(-1);
			}
            printf("Connection established\n");
		}
		else if(!strcmp(userInput, "get")) {
    			recvFile(filename);
		}
		else if(!strcmp(userInput, "post")) {
	    		sendFile(filename);
		}
		else if(!strcmp(userInput, "disconnect")) {
            rtp_terminate(sock);
			rtp_close(sock);
			exit(0);
		}
		else printf("Invalid command\n");
		

	}
}

void sendFile(char * buff) {
    printf("Checking to see if %s exists... \n", buff);
    //Open file, read from beginning
    FILE *code = fopen(buff, "rb");
    if(code == NULL) {
        printf("File not found\n");
        return;
    }
    printf("Telling server to expect to receive a file\n");
    sent = rtp_send(sock, "post", 4);
    if(sent < 0) {
    	printf("No connection to server");
        return;
    }
    
    char *contents;
    unsigned long size;
    //Set pointer to end of file
    fseek(code,0,SEEK_END);
    //Number of chars in file   
    size = ftello(code); 
    //Point to beginning of file
    fseek(code,0,SEEK_SET);
    
    contents = malloc(size+1);
    printf("File length, in bytes: %lu \n", size);
    //Read contents
    fread(contents, size, 1, code);
    //Close file stream
    fclose(code);
    printf("Sending name of file to server\n");
    //send name of file
    rtp_send(sock, buff, strlen(buff));
    contents[size] = '\0'; 
    unsigned long length = size;
    //send length of file
    printf("Sending length of file to server\n");
    rtp_send(sock, &length, sizeof(unsigned long));
    printf("Sending file contents\n");
    //send contents of file
    rtp_send(sock, contents, 1+size);
    free(contents);
	printf("File successfully sent\n");

}

void recvFile(char *buff) {
    sent = rtp_send(sock, "get", 3);
    if(sent < 0) {
        printf("Unable to send, make sure a connection has been established\n");
        return;
    }
    //send name of file
    printf("Sending the name of the file\n");
    int l = strlen(buff);
    //Send name of file
    rtp_send(sock, buff, l);
    unsigned long length = -1;
    printf("Waiting for file length\n");
    rtp_receive(sock, &length, sizeof(unsigned long));
    printf("File length: %lu\n", length);
    if(length <= 0) {
        printf("File doesn't exist at server\n");
        return;
    }
    buffer = malloc(length);
    printf("Retrieving file contents from server\n");
    struct timeval start, finish;
    gettimeofday(&start, NULL);
    //get file contents
    rtp_receive(sock, buffer, length);
    //write contents to file
    gettimeofday(&finish, NULL);
    float total = ((finish.tv_sec - start.tv_sec)*1000000L + finish.tv_usec) - start.tv_usec;
    total = total/1000000;
    printf("It took %f seconds to download the file\n", total);
    float throughput = (length/1000)/total;
    printf("Throughput was: %f kb/s\n", throughput);
    printf("Writing contents..\n");
    FILE *fp = fopen(buff, "wb");
    if(fp == NULL) {
        printf("File doesn't exist\n");
    }
    fwrite(buffer,sizeof(char), length, fp);
    fclose(fp);
	free(buffer);
    printf("File successfully received.\n");

}



