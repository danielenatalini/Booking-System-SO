CC = gcc
CFLAGS = -Wall -Wextra

all: server client

server: server.c archive.c users.c strutil.c common.h archive.h users.h strutil.h
	$(CC) $(CFLAGS) -o server server.c archive.c users.c strutil.c

client: client.c common.h
	$(CC) $(CFLAGS) -o client client.c

clean:
	rm -f server client *.o