#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#define MAX_STORAGE_SERVERS 10
#define MAX_CLIENTS 100
#define NAMING_SERVER_PORT 8000
#define HEARTBEAT_INTERVAL 5
#define MAX_CACHE_SIZE 100
#define LOCALIPADDRESS "192.168.1.1"

typedef struct FileSystem {
    // Simplified for example
    char fileTree[1000];  // Placeholder for file tree representation
} FileSystem;

typedef struct StorageServer {
    char ipAddress[16];  // IPv4 Address
    int nmPort;          // Port for NM Connection
    int clientPort;      // Port for Client Connection
    char accessiblePaths[1000]; // List of accessible paths
    // Other metadata as needed
} StorageServer;


int serializeStorageServer(StorageServer *server, char *buffer) {
    int offset = 0;
    offset += snprintf(buffer + offset, sizeof(server->ipAddress), "%s,", server->ipAddress);
    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "%d,%d,", server->nmPort, server->clientPort);
    offset += snprintf(buffer + offset, sizeof(server->accessiblePaths), "%s", server->accessiblePaths);
    return offset;
}

void sendStorageServer(StorageServer *server) {
    int sock;
    struct sockaddr_in server_addr;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(NAMING_SERVER_PORT);
    // server_addr.sin_addr.s_addr = inet_addr(LOCALIPADDRESS);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection to Naming Server failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    char buffer[1024];
    serializeStorageServer(server, buffer);

    if (send(sock, buffer, strlen(buffer), 0) < 0) {
        perror("Failed to send storage server data");
    }

    close(sock);
}

int main() {
    StorageServer myServer;
    // Fill in myServer details
    strcpy(myServer.ipAddress, "127.0.0.1");
    myServer.nmPort = 1234;
    myServer.clientPort = 5678;
    strcpy(myServer.accessiblePaths, "/path/to/data");

    sendStorageServer(&myServer);

    return 0;
}

