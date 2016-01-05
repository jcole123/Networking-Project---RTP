
CFLAGS = -g
SFLAGS = -g -pthread
AFLAGS = -D LINUX

all:	client server
client: fta-client.c rtp.o $(NETWORK_OBJECT)
	gcc $(CFLAGS) -o fta-client fta-client.c rtp.o
server: fta-server.c rtp.o $(NEWROK_OBJECT)
	gcc $(SFLAGS) -o fta-server fta-server.c rtp.o
rtp.o : rtp.c rtp.h
	gcc -c $(CFLAGS) $(AFLAGS) rtp.c
clean:
	rm -rf rtp.o fta-client fta-server

