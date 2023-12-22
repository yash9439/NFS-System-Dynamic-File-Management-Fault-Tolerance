CC = gcc
CFLAGS = -pthread
clean:
	rm -f s n client

all: s n client

s: StorageServerModule.c
	$(CC) $(CFLAGS) StorageServerModule.c -o s

n: NamingServerModule.c
	$(CC) $(CFLAGS) NamingServerModule.c -o n

client: clientfunctions.c
	$(CC) $(CFLAGS) clientfunctions.c -o client

