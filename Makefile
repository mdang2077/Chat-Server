CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -g -fsanitize=address

all: chat-server

chat-server: chat-server.c http-server.c http-server.h
	$(CC) $(CFLAGS) chat-server.c http-server.c -o chat-server

clean:
	rm -rf chat-server chat-server.dSYM

.PHONY: all clean
