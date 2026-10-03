CC = gcc
CFLAGS = -Wall -Wextra -std=gnu99

client1: client1.c clients.h
	$(CC) $(CFLAGS) -o client1 client1.c

