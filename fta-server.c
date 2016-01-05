#include <sys/socket.h>
#include "rtp.h"
#include <string.h> //memset
#include <sys/types.h>
#include <netinet/in.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h> //abort

void sendfile();
void recvfile();


int sock, c_size, port;
char input[50];

void poll_data() {
	while(1) {
		char userInput[200];
		memset(userInput, 0, 200);
		fgets(userInput, 200, stdin);
		strtok(userInput, "\n");
		if(strcmp(userInput, "terminate") == 0) {
			printf("Closing server..\n");
			rtp_close(sock);
			exit(0);
		}
	}
}

int main(int argc, char *argv[]) {
    struct sockaddr_in server;
    
	pthread_t user_poll;
	int x = 0;
    //Create thread to poll for user input
	if(pthread_create(&user_poll, NULL, (void*)poll_data, &x)) {
		fprintf(stderr, "Error creating thread\n");
		return 1;
	}
       
    if(argc < 4 || argc > 4) {
        printf("Invalid number of arguments \n");
        abort();
    }
    sock = conn_setup();
    if (socket < 0) {
        printf("Failed to create socket\n");
        exit(0);
    }
    memset(&server, 0, sizeof(server));
    port = atoi(argv[1]);
    
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port);

    if (bind(sock, (struct sockaddr *) &server, sizeof(server)) < 0) {
        printf("Unable to bind socket, aborting... \n");
        exit(0);
    }

    while(1) {
    	memset(input, 0, sizeof(input));
        rtp_receive(sock, input, sizeof(char)* 50);
        if(!strcmp(input, "post")) {
            recvfile();
            memset(input, 0, 50);
        }
        else if(!strcmp(input,"get")) {
            sendfile();
            memset(input, 0, 50);
        }
        else if(!strcmp(input, "connect")) {
            printf("Connection established\n");
            memset(input, 0, 50);
        }
        
    }
}

void sendfile() {
    char *swole = malloc(50);
    memset(swole, 0, 50);
    //Get name of file
    rtp_receive(sock, swole, 50);
    printf("Sending file: %s\n", swole);
    //Open file, read from beginning
    printf("File name: %s\n", swole);
    FILE *code = fopen(swole, "rb");
    if(code == NULL) {
        int nofile = -1;
        rtp_send(sock, &nofile, sizeof(int));
        return;
    }
    char *contents;
    int size;

    //Set pointer to end of file
    fseek(code,0,SEEK_END);
    //Number of chars in file   
    size = ftello(code);
    //Point to beginning of file
    fseek(code,0,SEEK_SET);
    unsigned long length = size;
    //send length of file
    printf("Sending the length of the file\n");
    rtp_send(sock, &length, sizeof(unsigned long));
    printf("File length: %lu\n", length);

    contents = malloc(size+1);
    memset(contents, 0, size+1);
    //Read contents
    fread(contents, size, 1, code);
    //Close file stream
    fclose(code);

    contents[size] = '\0';
    //send contents of file
    printf("Sending contents of file\n");
    rtp_send(sock, contents, 1+size);

    free(contents);
    free(swole);
    printf("File successfully sent\n");
}

void recvfile() {
    char *temp = malloc(sizeof(char) * 20);
    memset(temp, 0, 20);
    //get name of file
    rtp_receive(sock, temp, 20);
    printf("Name of file: %s\n", temp);
    //create empty file
    FILE *fp = fopen(temp, "wb");
    if(fp == NULL) {
        printf("File doesn't exist\n");
        return;
    }
    unsigned long length = 0;
    rtp_receive(sock, &length, sizeof(unsigned long));
    //Sanity check
    if(length <= 0) {
        printf("This should never happen.\n");
        return;
    }
    printf("Length of file: %lu \n", length);
    char* buffer = malloc(length + 1);
    memset(buffer, 0, length);
    printf("Waiting for file contents...\n");
    struct timeval start, finish;
    gettimeofday(&start, NULL);
    //get file contents
    rtp_receive(sock, buffer, length);
    gettimeofday(&finish, NULL);
    float total = ((finish.tv_sec - start.tv_sec)*1000000L + finish.tv_usec) - start.tv_usec;
    total = total/1000000;
    printf("It took %f seconds to download the file\n", total);
    float throughput = (length/1000)/total;
    printf("Throughput was: %f kb/s\n", throughput);
    buffer[length] = '\0';
    printf("File contents received\n");
    //write contents to file
    printf("Writing file\n");
    fwrite(buffer,sizeof(char), length, fp);
    //fputs(buffer, fp);
    //Close file pointer
    fclose(fp);
    free(buffer);
    printf("File successfully received\n");

}
