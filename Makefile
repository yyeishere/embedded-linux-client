CC = gcc
CFLAGS = -Wall -Wextra -std=gnu99

client1: client1.c clients.h
	$(CC) $(CFLAGS) -o client1 client1.c

client2: client2.c clients.h
	$(CC) $(CFLAGS) -o client2 client2.c

udptest: udptest.c
	$(CC) $(CFLAGS) -o udptest udptest.c
