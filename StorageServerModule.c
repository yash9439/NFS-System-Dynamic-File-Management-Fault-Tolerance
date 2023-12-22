#include <arpa/inet.h>
#include <dirent.h>
#include <libgen.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h> 
#include <sys/wait.h> 
#include <errno.h>


#define NMIPADDRESS "127.0.0.1"
#define SSIPADDRESS "127.0.0.2"

// Incomming Connection
// #define CLIENT_PORT 4000
// #define NM_PORT 5000
// #define SS_PORT 6000
#define MAX_DIRECTORIES 1000
#define MAX_PATH_LENGTH 1000
#define MAX_FILES 1000
#define MAX_PATH_LENGTH 1000

// OutGoing Connection
#define NAMING_SERVER_PORT 8000
#define MOUNT "."

// char NMIPADDRESS[16]; // Default value
// char SSIPADDRESS[16]; // For storing the IP address
int CLIENT_PORT;
int NM_PORT;
int SS_PORT_SEND;
int SS_PORT_RECV;

// // OutGoing Connection
// int NAMING_SERVER_PORT = 8000;
// char MOUNT[16] = "."; // For storing the MOUNT Path
typedef struct FileSystem
{
	// Simplified for example
	char fileTree[1000]; // Placeholder for file tree representation
} FileSystem;

typedef struct StorageServer
{
	char ipAddress[16]; // IPv4 Address
	int nmPort; // Port for NM Connection
	int clientPort; // Port for Client Connection
	int ssPort_send; // Port for SS Connection
	int ssPort_recv; // Port for SS Connection
	int numPaths;
	char accessiblePaths[500][100]; // List of accessible paths
		// Other metadata as needed
} StorageServer;


// LRU Caching
typedef struct LRUCache {
    StorageServer data;
	char keyPath[100];
    struct LRUCache *prev, *next;
} LRUCache;


LRUCache *head = NULL;
int cacheSize = 0;
int cacheCapacity = 0;


typedef struct
{
	char request[PATH_MAX]; // Adjust size as needed
	int socket;
} ThreadArg;

char files[MAX_FILES][MAX_PATH_LENGTH];
int fileCount = 0;
void handleClientRequest();
FileSystem fileSystem;
StorageServer ss;

void initializeLRUCache(int capacity) {
    head = NULL;
    cacheSize = 0;
    cacheCapacity = capacity;
}
int mkdir_p(const char *path) {
    char *copy = strdup(path);  // Make a copy to avoid modifying the original string
    char *ptr = copy;

    while (*ptr) {
        if (*ptr == '/') {
            *ptr = '\0';  // temporarily truncate the path
            if (mkdir(copy, S_IRWXU) != 0 && errno != EEXIST) {
                perror("mkdir");
                free(copy);
                return -1;
            }
            *ptr = '/';  // restore the path separator
        }
        ptr++;
    }

    if (mkdir(copy, S_IRWXU) != 0 && errno != EEXIST) {
        perror("mkdir");
        free(copy);
        return -1;
    }

    free(copy);
    return 0;
}

int accessStorageServerCache(char* keyPath) {
    LRUCache* temp = head;
    LRUCache* prevNode = NULL;

    // Search for the server in the cache
    while (temp != NULL && strcmp(temp->keyPath, keyPath) != 0) {
        prevNode = temp;
        temp = temp->next;
    }

    if (temp == NULL) { // Server not found in cache
		return 0;
    }
	if (prevNode != NULL) {
		prevNode->next = temp->next;
		if (temp->next != NULL) {
			temp->next->prev = prevNode;
		}
		temp->next = head;
		temp->prev = NULL;
		head->prev = temp;
		head = temp;
	}
	return 1;
}

void addServertoCache(char* keyPath, StorageServer server) {
	LRUCache* newNode = (LRUCache*)malloc(sizeof(LRUCache));
	newNode->data = server;
	strcpy(newNode->keyPath, keyPath);
	newNode->next = head;
	newNode->prev = NULL;

	if (head != NULL) {
	    head->prev = newNode;
	}
	head = newNode;

	if (cacheSize == cacheCapacity) { // Remove least recently used server
	    LRUCache* toRemove = head;
	    while (toRemove->next != NULL) {
	        toRemove = toRemove->next;
	    }
	    if (toRemove->prev != NULL) {
	        toRemove->prev->next = NULL;
	    }
	    free(toRemove);
	} else {
	    cacheSize++;
	}
}

void freeLRUCache() {
    LRUCache* current = head;
    while (current != NULL) {
        LRUCache* next = current->next;
        free(current);
        current = next;
    }
    head = NULL;
    cacheSize = 0;
}


int isDirectory(const char* path)
{
	struct stat fileStat;

	// Use stat to get information about the file or directory
	if(stat(path, &fileStat) == 0)
	{
		// Check if it's a directory
		if(S_ISDIR(fileStat.st_mode))
		{
			return 1; // True, it's a directory
		}
		else
		{
			return 0; // False, it's not a directory
		}
	}
	else
	{
		perror("Error getting file/directory information");
		return -1; // Error
	}
}

char directories[MAX_DIRECTORIES][MAX_PATH_LENGTH];
int directoryCount = 0;

void listDirectoriesRecursively(const char* path)
{
	DIR* dir;
	struct dirent* entry;

	// Store the current directory path in the array
	strncpy(directories[directoryCount], path, sizeof(directories[directoryCount]) - 1);
	directories[directoryCount][sizeof(directories[directoryCount]) - 1] = '\0';
	directoryCount++;

	// Open the directory
	if((dir = opendir(path)) != NULL)
	{
		// Read each entry in the directory
		while((entry = readdir(dir)) != NULL)
		{
			// Skip "." and ".."
			if(strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
			{
				// Construct the full path of the entry
				char fullpath[PATH_MAX];
				snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);

				// Check if the entry is a directory
				struct stat statbuf;
				if(stat(fullpath, &statbuf) == 0 && S_ISDIR(statbuf.st_mode))
				{
					// Recursively list directories within the subdirectory
					listDirectoriesRecursively(fullpath);

					// Ensure we don't exceed the array size
					if(directoryCount >= MAX_DIRECTORIES)
					{
						printf("Too many directories. Increase the array size.\n");
						break;
					}
				}
			}
		}

		// Close the directory
		closedir(dir);
	}
	else
	{
		perror("Error opening directory");
	}
}

void listFilesRecursively(const char* path)
{
	DIR* dir;
	struct dirent* entry;

	// Open the directory
	if((dir = opendir(path)) != NULL)
	{
		// Read each entry in the directory
		while((entry = readdir(dir)) != NULL)
		{
			// Skip "." and ".."
			if(strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
			{
				// Construct the full path of the entry
				char fullpath[PATH_MAX];
				snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);

				// Check if the entry is a regular file
				struct stat statbuf;
				if(stat(fullpath, &statbuf) == 0 && S_ISREG(statbuf.st_mode))
				{
					// Store the file path in the array
					strncpy(files[fileCount], fullpath, sizeof(files[fileCount]) - 1);
					files[fileCount][sizeof(files[fileCount]) - 1] = '\0';
					fileCount++;

					// Ensure we don't exceed the array size
					if(fileCount >= MAX_FILES)
					{
						printf("Too many files. Increase the array size.\n");
						break;
					}
				}

				// If it's a directory, recursively list files within the subdirectory
				else if(S_ISDIR(statbuf.st_mode))
				{
					listFilesRecursively(fullpath);
				}
			}
		}

		// Close the directory
		closedir(dir);
	}
	else
	{
		perror("Error opening directory");
	}
}
int deleteDirectory(char* path)
{
	DIR* dir;
	struct dirent* entry;
	char fullPath[PATH_MAX];

	// Open the directory
	if((dir = opendir(path)) == NULL)
	{
		perror("opendir");
		return -1;
	}

	// Iterate through each entry in the directory
	while((entry = readdir(dir)) != NULL)
	{
		if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
		{
			continue; // Skip "." and ".." entries
		}

		// Create full path to the entry
		snprintf(fullPath, sizeof(fullPath), "%s/%s", path, entry->d_name);

		// Recursively delete subdirectories
		if(entry->d_type == DT_DIR)
		{
			int ret = deleteDirectory(fullPath);
			if(ret ==-1 )
			{
				return ret;
			}
		}
		else
		{
			// Delete regular files
			if(unlink(fullPath) != 0)
			{
				perror("unlink");
				return -1;
			}
		}
	}

	// Close the directory
	closedir(dir);

	// Remove the directory itself
	if(rmdir(path) != 0)
	{
		perror("rmdir");
		return -1;
	}
}
char* getDirectoryPath(const char* path)
{
	int length = strlen(path);

	// Find the last occurrence of '/'
	int lastSlashIndex = -1;
	for(int i = length - 1; i >= 0; i--)
	{
		if(path[i] == '/')
		{
			lastSlashIndex = i;
			break;
		}
	}

	if(lastSlashIndex != -1)
	{
		// Allocate memory for the new string
		char* directoryPath = (char*)malloc((lastSlashIndex + 1) * sizeof(char));

		// Copy the directory path into the new string
		strncpy(directoryPath, path, lastSlashIndex);

		// Add null terminator
		directoryPath[lastSlashIndex] = '\0';

		return directoryPath;
	}
	else
	{
		// No '/' found, the path is already a directory
		return strdup(path); // Duplicate the string to ensure the original is not modified
	}
}

char* replacePrefix(char* originalString,
					const char* prefixToReplace,
					const char* replacementString)
{
	// Find the position of the prefix in the original string
	char* position = strstr(originalString, prefixToReplace);

	if(position != NULL)
	{
		// Calculate the length of the prefix
		size_t prefixLength = position - originalString;

		// Calculate the length of the remaining part after the prefix
		size_t remainingLength = strlen(originalString) - prefixLength;

		// Allocate memory for the new string
		char* newString =
			(char*)malloc(prefixLength + strlen(replacementString) + remainingLength + 1);

		if(newString != NULL)
		{
			// Copy the replacement string
			strncpy(newString, replacementString, strlen(replacementString));
			newString[strlen(replacementString)] = '\0';

			// Concatenate the remaining part of the original string
			strcat(newString, position + strlen(prefixToReplace));

			return newString;
		}
		else
		{
			perror("Memory allocation error");
			exit(EXIT_FAILURE);
		}
	}
	else
	{
		// If the prefix is not found, return a copy of the original string
		return strdup(originalString);
	}
}

void update_accessible_paths_recursive(char* path)
{
	DIR* dir;
	struct dirent* entry;

	// Open the directory
	dir = opendir(path);

	if(dir == NULL)
	{
		perror("opendir");
		return;
	}

	// Iterate over entries in the directory
	while((entry = readdir(dir)) != NULL)
	{
		// Skip "." and ".."
		if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
		{
			continue;
		}

		// Create the full path of the entry
		char full_path[PATH_MAX];
		snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

		// update to accessible paths
		strcpy(ss.accessiblePaths[ss.numPaths], full_path);
		printf("PATH %s\n ", full_path);
		ss.numPaths++;

		// If it's a directory, recursively call the function
		if(entry->d_type == DT_DIR)
		{
			update_accessible_paths_recursive(full_path);
		}
	}

	// Close the directory
	closedir(dir);
}
void registerStorageServer(char* ipAddress, int nmPort, int clientPort, char* accessiblePaths)
{
	strcpy(ss.ipAddress, ipAddress);
	ss.nmPort = nmPort;
	ss.clientPort = clientPort;
	ss.ssPort_send = SS_PORT_SEND;
	ss.ssPort_recv = SS_PORT_RECV;
	strcpy(ss.accessiblePaths[0], MOUNT);
	// update initial accessible paths
	ss.numPaths = 1;
	// char * path ;
	// strcpy(path,accessiblePaths);
	update_accessible_paths_recursive(accessiblePaths);

	// for (int i = 0; i < ss.numPaths; i++) {
	//     printf("%s\n", ss.accessiblePaths[i]);
	// }
}

void initializeStorageServer()
{
	// Initialize SS_1
	registerStorageServer(SSIPADDRESS, NM_PORT, CLIENT_PORT, MOUNT);
}

int serializeStorageServer(StorageServer* server, char* buffer)
{
	int offset = 0;
	offset += snprintf(buffer + offset, sizeof(server->ipAddress), "%s,", server->ipAddress);
	offset += snprintf(
		buffer + offset, sizeof(buffer) - offset, "%d,%d,%d,%d,", server->nmPort, server->clientPort, server->ssPort_send, server->ssPort_recv);
	offset +=
		snprintf(buffer + offset, sizeof(server->accessiblePaths), "%s", server->accessiblePaths);
	return offset;
}

// Function for the storage server to report its status to the naming server.
void reportToNamingServer(StorageServer* server)
{
	int sock;
	struct sockaddr_in server_addr;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock < 0)
	{
		perror("Socket creation failed");
		exit(EXIT_FAILURE);
	}

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(NAMING_SERVER_PORT);
	server_addr.sin_addr.s_addr = inet_addr(NMIPADDRESS);

	if(connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
	{
		perror("Connection to Naming Server failed");
		close(sock);
		exit(EXIT_FAILURE);
	}

	char buffer[sizeof(StorageServer)];
	printf("Size of StorageServer : %d\n", sizeof(StorageServer));
	printf("Size of buffer : %d\n", sizeof(buffer));
	printf("Size of server : %d\n", sizeof(*server));
	printf("address of server : %d\n", server->numPaths);
	memcpy(buffer, server, sizeof(StorageServer));
	if(send(sock, buffer, sizeof(buffer), 0) < 0)
	{
		perror("Failed to send storage server data");
	}

	close(sock);
}

void* executeClientRequest(void* arg)
{
	ThreadArg* threadArg = (ThreadArg*)arg;
	char* request = threadArg->request;
	int clientSocket = threadArg->socket;

	char command[PATH_MAX], path[PATH_MAX];
	memset(command, '\0', sizeof(command));
	memset(path, '\0', sizeof(path));
	sscanf(request, "%s %s", command, path);

	printf("Command: %s %s\n", command, path);

	if(strcmp(command, "READ") == 0)
	{
		FILE* file = fopen(path, "r");
		if(file == NULL)
		{
			perror("Error opening file");
			const char* errMsg = "Failed to open file";
			printf("Error :  %s\n", errMsg);
			send(clientSocket, errMsg, strlen(errMsg), 0);
		}
		else
		{
			char fileContent[4096]; // Adjust size as needed
			memset(fileContent, '\0', sizeof(fileContent));
			size_t bytesRead = fread(fileContent, 1, sizeof(fileContent) - 1, file);
			if(ferror(file))
			{
				perror("Read error");
				printf("Error :  %s\n", "Read error");
				send(clientSocket, "Read error", 10, 0);
			}
			else if(bytesRead == 0)
			{
				printf("No content\n");
				send(clientSocket, "No content", 10, 0);
			}
			else
			{
				fileContent[bytesRead] = '\0';
				printf("File content: %s\n", fileContent);
				send(clientSocket, fileContent, bytesRead, 0);
			}
			fclose(file);
		}
	}
	else if(strcmp(command, "GETSIZE") == 0)
	{
		struct stat statbuf;
		if(stat(path, &statbuf) == -1)
		{
			perror("Error getting file size");
			send(clientSocket, "Error getting file size", 23, 0);
		}
		else
		{

			struct tm *tm;
			char last_modified[30], last_accessed[30];
			memset(last_accessed, '\0', sizeof(last_accessed));
			memset(last_modified, '\0', sizeof(last_modified));

			// Convert last modification time
			tm = localtime(&statbuf.st_mtime);
			strftime(last_modified, sizeof(last_modified), "%Y-%m-%d %H:%M:%S", tm);

			// Convert last accessed time
			tm = localtime(&statbuf.st_atime);
			strftime(last_accessed, sizeof(last_accessed), "%Y-%m-%d %H:%M:%S", tm);


			char response[PATH_MAX];
			char permissions[11];
			// set memory     
			memset(response, '\0', sizeof(response));
			memset(permissions, '\0', sizeof(permissions));

			// Convert st_mode to a permissions string
			snprintf(permissions,
					 sizeof(permissions),
					 "%c%c%c%c%c%c%c%c%c%c",
					 (S_ISDIR(statbuf.st_mode)) ? 'd' : '-',
					 (statbuf.st_mode & S_IRUSR) ? 'r' : '-',
					 (statbuf.st_mode & S_IWUSR) ? 'w' : '-',
					 (statbuf.st_mode & S_IXUSR) ? 'x' : '-',
					 (statbuf.st_mode & S_IRGRP) ? 'r' : '-',
					 (statbuf.st_mode & S_IWGRP) ? 'w' : '-',
					 (statbuf.st_mode & S_IXGRP) ? 'x' : '-',
					 (statbuf.st_mode & S_IROTH) ? 'r' : '-',
					 (statbuf.st_mode & S_IWOTH) ? 'w' : '-',
					 (statbuf.st_mode & S_IXOTH) ? 'x' : '-');

			// Append additional metadata to the response
			snprintf(response,
					sizeof(response),
					"Size of %s: %ld bytes, Permissions: %s, Last Modified: %s, Last Accessed: %s",
					path,
					statbuf.st_size,
					permissions,
					last_modified,
					last_accessed);

			// Send the response to the client
			send(clientSocket, response, strlen(response), 0);
		}
	}
	else if(strcmp(command, "WRITE") == 0)
	{
		char content[4096]; // Adjust size as needed
		memset(content, '\0', sizeof(content));
		sscanf(request, "%*s %*s %[^\t\n]", content); // Reads the content part of the request

		FILE* file = fopen(path, "w");
		if(file == NULL)
		{
			perror("Error opening file for writing");
			send(clientSocket, "Error opening file for writing", 30, 0);
		}
		else
		{
			size_t bytesWritten = fwrite(content,sizeof(char),strlen(content),file);
			if(ferror(file))
			{
				perror("Write error");
				send(clientSocket, "Write error", 11, 0);
			}
			else
			{
				char response[1024];
				memset(response, '\0', sizeof(response));
				snprintf(response, sizeof(response), "Written %ld bytes to %s", bytesWritten, path);
				printf("Written %ld bytes to %s\n", bytesWritten, path);
				send(clientSocket, response, strlen(response), 0);
			}
			fclose(file);
		}
	}
	else
	{
		printf("Invalid command\n");
	}

	free(threadArg); // Free the allocated memory
	close(clientSocket); // Close the connection
	return NULL;
}
void getSubstringBeforeLastSlash(const char *input, char *output, size_t outputSize) {
    const char *slashPosition = strrchr(input, '/');

    if (slashPosition != NULL) {
        size_t length = slashPosition - input;
        if (length < outputSize) {
            strncpy(output, input, length);
            output[length] = '\0';  // Null-terminate the output string
        } else {
            fprintf(stderr, "Output buffer too small for substring.\n");
        }
    } else {
        // No '/' found, copy the whole string
        strncpy(output, input, outputSize - 1);
        output[outputSize - 1] = '\0';  // Null-terminate the output string
    }
}



void* executeNMRequest(void* arg)
{
	ThreadArg* threadArg = (ThreadArg*)arg;
	char* request = threadArg->request;
	int NMSocket = threadArg->socket;

	char command[PATH_MAX];
	char path[PATH_MAX]; // Source path
	char path2[PATH_MAX]; // Destination path
	char destination_ip[PATH_MAX]; // Destination IP
	char destination_port[100]; // Destination server port
	// memset all
	memset(command, '\0', sizeof(command));
	memset(path, '\0', sizeof(path));
	memset(path2, '\0', sizeof(path2));
	memset(destination_ip, '\0', sizeof(destination_ip));
	memset(destination_port, '\0', sizeof(destination_port));

	printf("Request: %s\n", request);
	sscanf(request, "%s %s", command, path);
	if(strcmp(command, "COPY") == 0)
	{
		// printf("GOT here  %s\n", request);
		sscanf(request, "%s %s %s %s %s", command, destination_ip, destination_port, path, path2);
		printf("Command: %s %s %s %s %s\n", command, destination_ip, destination_port, path, path2);
		// printf("GOT COPY\n");
	}
	else
	{
		sscanf(request, "%s %s", command, path);

		// printf("Command1: %s %s\n", command, path);
		// printf("Command2: %s\n", path);
		// printf("Command3: %s\n", command);
	}

	if(strcmp(command, "GETPATHS") == 0)
	{
		// send the number of paths to the nm
		memset(ss.accessiblePaths, '\0', sizeof(ss.accessiblePaths));
		// Add more conditions as needed
		ss.numPaths = 1;
		strcpy(ss.accessiblePaths[0], MOUNT);
		update_accessible_paths_recursive(MOUNT);
		char response[PATH_MAX];
		memset(response, '\0', sizeof(response));
		sprintf(response, "%d", ss.numPaths);
		if(send(NMSocket, response, strlen(response), 0) < 0)
		{
			perror("Send failed");
			exit(EXIT_FAILURE);
		}

		// recieve the response from the nm
		char buffer[PATH_MAX];
		int totalRead = 0;
		memset(buffer, '\0', sizeof(buffer));
		if(recv(NMSocket, buffer, sizeof(buffer), 0) < 0)
		{
			perror("recv failed");
			exit(EXIT_FAILURE);
		}
		if(strcmp(response, buffer) != 0)
		{
			printf("Error in copying the files\n");
			memset(response, '\0', sizeof(response));
			strcpy(response, "1");
			printf("Response right: %s\n", response);
			printf("Response wrong: %s\n", buffer);
			send(NMSocket, response, strlen(response), 0);
			return NULL;
		}
		else
		{
			memset(response, '\0', sizeof(response));
			strcpy(response, "OK");
			send(NMSocket, response, strlen(response), 0);
		}
		for (int _path =0; _path < ss.numPaths; _path++)
		{
			// send the path to the nm
			if(send(NMSocket, ss.accessiblePaths[_path], strlen(ss.accessiblePaths[_path]), 0) < 0)
			{
				perror("Send failed");
				exit(EXIT_FAILURE);
			}
			// recieve the response from the nm
			char buffer2[PATH_MAX];
			totalRead = 0;
			memset(buffer2, '\0', sizeof(buffer2));
			if(recv(NMSocket, buffer2, sizeof(buffer2), 0) < 0)
			{
				perror("recv failed");
				exit(EXIT_FAILURE);
			}
			if(strcmp("OK", buffer2) != 0)
			{
				printf("Error in copying the files\n");
				memset(response, '\0', sizeof(response));
				strcpy(response, "2");
				return NULL;
			}
		}
	}
	else if(strcmp(command, "CREATE") == 0)
	{
		int pathLength = strlen(path);
		printf("Path length: %s\n", path);
		char response[PATH_MAX];
		if(path[pathLength - 1] == '/')
		{ // Check if the path ends with '/'
			// Create the directory
			if(mkdir_p(path) == 0)
			{
				printf("Directory created: %s\n", path);
				strcpy(response, "1");
			}
			else
			{
				perror("Error creating directory");
				strcpy(response, "2");
			}
		}
		else
		{ // Treat as file
			char lastsub[1000];
			getSubstringBeforeLastSlash(path, lastsub, sizeof(lastsub));
			printf("lastsub %s\n", lastsub);
			if(mkdir_p(lastsub) == 0)
			{
				printf("Directory created: %s\n", lastsub);
				strcpy(response, "1");
			}
			else
			{
				perror("Error creating directory");
				strcpy(response, "2");
			}
			FILE* file = fopen(path, "w");
			if(file == NULL)
			{
				perror("Error creating file");
				strcpy(response, "3");
			}
			else
			{
				printf("File created: %s\n", path);
				strcpy(response, "4");
				fclose(file);
			}
		}
		send(NMSocket, response, strlen(response), 0);
	}
	else if(strcmp(command, "DELETE") == 0)
	{
		struct stat path_stat;
		stat(path, &path_stat);
		char response[PATH_MAX];
		if(S_ISDIR(path_stat.st_mode))
		{ // Check if it's a directory
			// rmdir only works on empty directories. For non-empty directories, you'll need a more complex function
			int ret = deleteDirectory(path);
			if(ret == -1)
			{
				perror("Error deleting directory");
				strcpy(response, "-1");
			}
			else
			{
				strcpy(response, "2");
			}
			printf("Directory deleted: %s\n", path);
		}
		else
		{ // Treat as file
			if(remove(path) == 0)
			{
				printf("File deleted: %s\n", path);
				strcpy(response, "3");
			}
			else
			{
				perror("Error deleting file");
				strcpy(response, "-1");
			}
		}
		send(NMSocket, response, strlen(response), 0);
	}
	else if(strncmp(command, "COPY", strlen("COPY")) == 0)
	{
		int d_port = atoi(destination_port);

		// open the file in read mode which path is "path"
		FILE* fptr1 = fopen(path, "r");
		int is_dir = isDirectory(path);
		if(is_dir == 1)
		{
			printf("copy a directory\n");
			// now we need to send to storage server whether the incoming file is a directory or not
			char response[PATH_MAX];
			memset(response, '\0', sizeof(response));
			strcpy(response, "1");
			int sock;
			struct sockaddr_in serverAddr;
			// create a socket
			sock = socket(AF_INET, SOCK_STREAM, 0);
			if(sock < 0)
			{
				perror("Could not create socket");
				//send this to nm as error
				memset(response, '\0', sizeof(response));
				strcpy(response, "19");
				send(NMSocket, response, strlen(response), 0);
				close(sock);
				return NULL;
			}
			// set the server address
			serverAddr.sin_addr.s_addr = inet_addr(destination_ip);
			serverAddr.sin_family = AF_INET;
			serverAddr.sin_port = htons(d_port);
			// connect to the destination server
			if(connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0)
			{
				perror("Connect failed. Error");
				memset(response, '\0', sizeof(response));

				strcpy(response, "2");
				//send this to nm as error
				send(NMSocket, response, strlen(response), 0);
				close(sock);
				return NULL;
			}
			// send the file path to the destination server
			if(send(sock, response, strlen(response), 0) < 0)
			{
				perror("Send failed");
				close(sock);
				return NULL;
			}

			// recieve the response from the destination server
			char buffer[PATH_MAX];
			int totalRead = 0;
			memset(buffer, '\0', sizeof(buffer));
			if(recv(sock, buffer, sizeof(buffer), 0) < 0)
			{
				perror("recv failed");
				close(sock);
				exit(EXIT_FAILURE);
			}

			if(strncmp(buffer, "FOLDER", strlen("FOLDER")) != 0)
			{
				printf("Error in Identifying FOLDER \n");
				close(sock);
				memset(response, '\0', sizeof(response));
				strcpy(response, "3");
				send(NMSocket, response, strlen(response), 0);
				return NULL;
			}
			printf("Server reply: %s\n", buffer);
			// now create the directory in the destination server
			directoryCount = 0;
			memset(directories, '\0', sizeof(directories));
			listDirectoriesRecursively(path);
			//now send the number of directories to the destination server
			char response2[PATH_MAX];
			memset(response2, '\0', sizeof(response2));
			sprintf(response2, "%d", directoryCount);
			if(send(sock, response2, strlen(response2), 0) < 0)
			{
				perror("Send failed");
				close(sock);
				exit(EXIT_FAILURE);
			}
			//now recieve the response from the destination server
			char buffer2[PATH_MAX];
			totalRead = 0;
			if(recv(sock, buffer2, sizeof(buffer2), 0) < 0)
			{
				perror("recv failed");
				close(sock);
				exit(EXIT_FAILURE);
			}
			// check the number of directories from the destination server
			printf("Server reply: %s\n", buffer2);
			if(strcmp(response2, buffer2) != 0)
			{
				printf("Error in copying the directory\n");
				memset(response, '\0', sizeof(response));
				strcpy(response, "4");
				send(NMSocket, response, strlen(response), 0);
				close(sock);
				return NULL;
			}

			// now send the directories to the destination server with replaced source parent with destination parent
			char* parent_source = getDirectoryPath(path);

			// now in a for loop send all the directories to the destination server
			for(int dir_ = 0; dir_ < directoryCount; dir_++)
			{
				char* parent_destination = getDirectoryPath(directories[dir_]);
				char* new_path = replacePrefix(directories[dir_], parent_source, path2);
				printf("new path %s\n", new_path);
				// send the file path to the destination server
				if(send(sock, new_path, strlen(new_path), 0) < 0)
				{
					perror("Send failed");
					close(sock);
					exit(EXIT_FAILURE);
				}
				// recieve the response from the destination server
				char buffer3[PATH_MAX];
				totalRead = 0;
				if(recv(sock, buffer3, sizeof(buffer3), 0) < 0)
				{
					perror("recv failed");
					close(sock);
					exit(EXIT_FAILURE);
				}
				printf("Server reply: %s\n", buffer3);
				if(strncmp(buffer3, "OK", strlen("OK")) != 0)
				{
					printf("Error in copying the directory\n");
					memset(response, '\0', sizeof(response));
					strcpy(response, "5");
					send(NMSocket, response, strlen(response), 0);
					close(sock);
					// exit(EXIT_FAILURE);
					return NULL;
				}
			}

			// now we need to send the files to the destination server
			fileCount = 0;
			memset(files, '\0', sizeof(files));
			listFilesRecursively(path);
			// now we need to send the number of files to the destination server
			char response3[PATH_MAX];
			memset(response3, '\0', sizeof(response3));
			sprintf(response3, "%d", fileCount);
			if(send(sock, response3, strlen(response3), 0) < 0)
			{
				perror("Send failed");
				close(sock);
				exit(EXIT_FAILURE);
			}
			//now recieve the response from the destination server
			char buffer4[PATH_MAX];
			totalRead = 0;
			if(recv(sock, buffer4, sizeof(buffer4), 0) < 0)
			{
				perror("recv failed");
				close(sock);
				exit(EXIT_FAILURE);
			}
			// check the number of files from the destination server
			printf("Server reply: %s\n", buffer4);
			if(strcmp(response3, buffer4) != 0)
			{
				printf("Error in copying the files\n");
				memset(response, '\0', sizeof(response));
				strcpy(response, "6");
				send(NMSocket, response, strlen(response), 0);

				close(sock);
				exit(EXIT_FAILURE);
			}
			// now send the files to the destination server with replaced source parent with destination parent
			for(int _file = 0; _file < fileCount; _file++)
			{
				char* parent_source = getDirectoryPath(path);
				char* new_path = replacePrefix(files[_file], parent_source, path2);
				printf("new path %s\n", new_path);
				// send the file path to the destination server
				if(send(sock, new_path, strlen(new_path), 0) < 0)
				{
					perror("Send failed");
					close(sock);
					exit(EXIT_FAILURE);
				}
				// recieve the response from the destination server
				char buffer5[PATH_MAX];
				totalRead = 0;
				if(recv(sock, buffer5, sizeof(buffer5), 0) < 0)
				{
					perror("recv failed");
					close(sock);
					exit(EXIT_FAILURE);
				}
				printf("Server reply: %s\n", buffer5);
				if(strncmp(buffer5, "OK", strlen("OK")) != 0)
				{
					printf("Error in copying the directory\n");
					close(sock);
					exit(EXIT_FAILURE);
				}
				// now we need to recieve the file from the source server
				char buffer6[PATH_MAX];
				totalRead = 0;
				// read teh file from the source server
				FILE* file = fopen(files[_file], "r");
				if(file == NULL)
				{
					printf("Cannot open file %s \n", files[_file]);
					send(NMSocket, "13", strlen("8"), 0);
					return NULL;
				}
				// copy the contents of the file in buffer to send it to the destination server
				char bufferread[PATH_MAX];
				int nread = fread(bufferread, 1, sizeof(bufferread), file);
				printf("%s is the buffer\n", buffer);
				// close the file
				fclose(file);

				// now we need to send the file to the destination server but first we need to connect to the destination server and send the file path
				if(send(sock, bufferread, nread, 0) < 0)
				{
					perror("Send failed");
					close(sock);
					exit(EXIT_FAILURE);
				}
				// recieve the response from the destination server
				char buffer7[PATH_MAX];
				totalRead = 0;
				if(recv(sock, buffer7, sizeof(buffer7), 0) < 0)
				{
					perror("recv failed");
					close(sock);
					exit(EXIT_FAILURE);
				}
				printf("Server reply: %s\n", buffer7);
				if(strncmp(buffer7, "1", strlen("1")) == 0)
				{
					printf("Error in copying the directory\n");
					memset(response, '\0', sizeof(response));
					strcpy(response, "7");
					send(NMSocket, response, strlen(response), 0);
					close(sock);
					return NULL;
				}
			}
		}
		else
		{
			// printf("Command: %s %s %s %s\n", destination_ip, destination_port, path, path);
			// check if the file is opened or not
			if(fptr1 == NULL)
			{
				printf("Cannot open file %s \n", path);

				// send the error to the nm
				char response[PATH_MAX];
				memset(response, '\0', sizeof(response));
				strcpy(response, "8");
				send(NMSocket, response, strlen(response), 0);
				return NULL;
			}

			// copy the contents of the file in buffer to send it to the destination server
			char buffer[PATH_MAX];
			int nread = fread(buffer, 1, sizeof(buffer), fptr1);
			// printf("%s is the buffer\n", buffer);
			// close the file
			fclose(fptr1);
			// now we need to send the file to the destination server but first we need to connect to the destination server and send the file path
			// and then we need to send the file to the destination server
			// create a socket
			int sock;
			struct sockaddr_in serverAddr;
			// create a socket
			sock = socket(AF_INET, SOCK_STREAM, 0);
			if(sock < 0)
			{
				perror("Could not create socket");
				return NULL;
			}
			// set the server address
			serverAddr.sin_addr.s_addr = inet_addr(destination_ip);
			serverAddr.sin_family = AF_INET;
			serverAddr.sin_port = htons(d_port);
			// connect to the destination server
			if(connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0)
			{
				send(NMSocket, "2", strlen("2"), 0);
				perror("Connect failed. Error");
				return NULL;
			}
			if(send(sock, "0", 1, 0) < 0)
			{
				perror("Send failed");
				close(sock);
				exit(EXIT_FAILURE);
			}
			// recieve the response from the destination server
			char buffer2[PATH_MAX];
			memset(buffer2, '\0', sizeof(buffer2));
			int totalRead = 0;
			if(recv(sock, buffer2, sizeof(buffer2), 0) < 0)
			{
				perror("recv failed");
				close(sock);
				exit(EXIT_FAILURE);
			}
			// printf("Server reply: %s\n", buffer2);
			// send the file path to the destination server
			if(send(sock, path2, strlen(path2), 0) < 0)
			{
				perror("Send failed");
				close(sock);
				exit(EXIT_FAILURE);
			}
			// recieve the response from the destination server
			memset(buffer2, '\0', sizeof(buffer2));
			totalRead = 0;
			if(recv(sock, buffer2, sizeof(buffer2), 0) < 0)
			{
				perror("recv failed");
				close(sock);
				exit(EXIT_FAILURE);
			}
			// printf("Server reply: %s\n", buffer2);
			if(strncmp(buffer2, "OK", strlen("OK")) == 0)
			{
				// send the file to the destination server
				printf("Sending file to the destination server\n");
				// printf("GOT OK\n");
				if(send(sock, buffer, nread, 0) < 0)
				{
					perror("Send failed");
					close(sock);
					exit(EXIT_FAILURE);
				}
				// recieve the response from the destination server
				char buffer3[PATH_MAX];
				int totalRead = 0;
				if(recv(sock, buffer3, sizeof(buffer3), 0) < 0)
				{
					perror("recv failed");
					close(sock);
					exit(EXIT_FAILURE);
				}
				printf("Server reply: %s\n", buffer3);
			}
			else
			{
				char response[PATH_MAX];
				printf("Error in copying the file\n");
				memset(response, '\0', sizeof(response));
				strcpy(response, "6");

				printf("Server reply: %s\n", buffer2);
				send(NMSocket, response, strlen(response), 0);
				close(sock);
				return NULL;

			}
		}

		// send the ack to nm
		printf("Sending ack to nm\n");
		char response[PATH_MAX];
		memset(response, '\0', sizeof(response));
		strcpy(response, "11");
		if(send(NMSocket, response, strlen(response), 0) < 0)
		{
			perror("Send failed");
			close(NMSocket);
			exit(EXIT_FAILURE);
		}
	}
	free(threadArg); // Free the allocated memory
	close(NMSocket); // Close the connection
	return NULL;
}

void* executeSSRequestRecv(void * arg) {
    ThreadArg* threadArg = (ThreadArg*)arg;
    int connSock = threadArg->socket;
    const char * path = threadArg->request;
    
    char buffer[PATH_MAX];
    ssize_t bytesReceived;

    char parentPath[PATH_MAX];
    char targetDir[PATH_MAX];
    char zipPath[PATH_MAX];

	// Memset all
	memset(parentPath, '\0', sizeof(parentPath));
	memset(targetDir, '\0', sizeof(targetDir));
	memset(zipPath, '\0', sizeof(zipPath));

    // Split the path into parent and target directory
    strncpy(parentPath, path, PATH_MAX);
    strncpy(targetDir, path, PATH_MAX);
    char *parentDir = dirname(parentPath);
    char *target = basename(targetDir);

    printf("REC Path: %s\n", path);
	char temp[PATH_MAX];
	memset(temp, '\0', sizeof(temp));
	strcpy(temp, path);
	strcat(temp, "1.zip");
    // Open or create file at path
    int filefd = open(temp, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
    if (filefd < 0) {
        perror("Failed to open file");
        return NULL;
    }

    // Receive data from NM and write to file
	while (1) {
		ssize_t bytesReceived = recv(connSock, buffer, sizeof(buffer), 0);
		if (bytesReceived > 0) {
			printf("Bytes received: %ld\n", bytesReceived);
			ssize_t bytesWritten = write(filefd, buffer, bytesReceived);
			if (bytesWritten < bytesReceived) {
				perror("Failed to write complete data to file");
				send(connSock, "ERROR", strlen("ERROR"), 0);
				break;
			}
			send(connSock, "", strlen(""), 0);
		} else if (bytesReceived == 0) {
			printf("Connection closed by peer\n");
			break;
		} else {
			perror("recv failed");
			break;
		}
	}
	printf("Out1\n");

    if (bytesReceived < 0) {
        perror("Failed to receive data");
    }

    close(filefd);

	char unzippedDir[PATH_MAX];
	snprintf(unzippedDir, sizeof(unzippedDir), "%s", path);

	printf("Unzipping to %s\n", unzippedDir);

	// Prepare unzip command
	char unzipCommand[PATH_MAX + 50];
	snprintf(unzipCommand, sizeof(unzipCommand), "unzip '%s' -d '%s' ; rm '%s'", temp, unzippedDir, temp);
	printf("Unzip command: %s\n", unzipCommand);
    // Execute the unzip command
    int result = system(unzipCommand);

	printf("Unzipping complete\n");


	return NULL;
}

void* executeSSRequest(void* arg)
{
    ThreadArg* threadArg = (ThreadArg*)arg;
    int connSock = threadArg->socket;
    const char *path = threadArg->request;

    char parentPath[PATH_MAX];
    char targetDir[PATH_MAX];
    char zipPath[PATH_MAX];

	// Memset all
	memset(parentPath, '\0', sizeof(parentPath));
	memset(targetDir, '\0', sizeof(targetDir));
	memset(zipPath, '\0', sizeof(zipPath));

    // Split the path into parent and target directory
    strncpy(parentPath, path, PATH_MAX);
    strncpy(targetDir, path, PATH_MAX);
    char *parentDir = dirname(parentPath);
    char *target = basename(targetDir);

    // Create zip file path
    snprintf(zipPath, sizeof(zipPath), "%s.zip", target);

    // Prepare zip command
    char zipCommand[PATH_MAX + 50];
	memset(zipCommand, '\0', sizeof(zipCommand));

    snprintf(zipCommand, sizeof(zipCommand), "cd '%s' && zip -r '%s' '%s' > /dev/null 2>&1", parentDir, zipPath, target);

    // Execute the zip command
    int zipStatus = system(zipCommand);
    if (zipStatus != 0) {
        perror("Failed to zip directory");
        close(connSock);
        return (void*)2;
    }

    // Update path to the zip file
    snprintf(zipPath, sizeof(zipPath), "%s/%s.zip", parentDir, target);
    path = zipPath;

    char buffer[PATH_MAX];
    ssize_t bytesRead;

    // Open the file or directory at path
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        perror("Failed to open file");
        close(connSock);  // Close the socket if file open fails
        return (void*)1;  // Return an error code
    }

    // Read from file and send to NM
    while ((bytesRead = fread(buffer, 1, PATH_MAX, file)) > 0) {
        ssize_t bytesSent = send(connSock, buffer, bytesRead, 0);
        if (bytesSent < 0) {
            perror("Failed to send data");
            break;
        }
    }

	// delete the zip file
	char deleteCommand[PATH_MAX + 50];
	memset(deleteCommand, '\0', sizeof(deleteCommand));
	snprintf(deleteCommand, sizeof(deleteCommand), "rm '%s'", zipPath);
	int deleteStatus = system(deleteCommand);
	if (deleteStatus != 0) {
		perror("Failed to delete zip file");
		close(connSock);
		return (void*)3;
	}
	
	printf("Out2\n");

    fclose(file);
    close(connSock); // Close the socket after sending data

    return (void*)0;  // Return success

}

void* handleClientConnections(void* args)
{
	int server_fd, new_socket, opt = 1;
	struct sockaddr_in address;
	int addrlen = sizeof(address);

	// Creating socket file descriptor
	if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
	{
		perror("socket failed");
		exit(EXIT_FAILURE);
	}

	// Forcefully attaching socket to the CLIENT_PORT
	if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
	{
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = inet_addr(SSIPADDRESS); // Listening on SSIPADDRESS
	address.sin_port = htons(CLIENT_PORT);

	// Bind the socket to the port
	if(bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0)
	{
		perror("bind failed");
		exit(EXIT_FAILURE);
	}

	// Listen for incoming connections
	if(listen(server_fd, 3) < 0)
	{
		perror("listen");
		exit(EXIT_FAILURE);
	}

	while(1)
	{
		if((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0)
		{
			perror("accept");
			continue;
		}

		// Execute the Command in a new thread so that SS is always listening for new connections
		ThreadArg* arg = malloc(sizeof(ThreadArg));
		memset(arg->request, '\0', sizeof(arg->request));
		read(new_socket, arg->request, sizeof(arg->request));
		arg->socket = new_socket;

		pthread_t tid;
		if(pthread_create(&tid, NULL, (void* (*)(void*))executeClientRequest, arg) != 0)
		{
			perror("Failed to create thread for client request");
		}

		pthread_detach(tid); // Detach the thread
	}

	return NULL;
}

void* handleNamingServerConnections(void* args)
{
	int server_fd, new_socket, opt = 1;
	struct sockaddr_in address;
	int addrlen = sizeof(address);

	// Creating socket file descriptor
	if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
	{
		perror("socket failed");
		exit(EXIT_FAILURE);
	}

	// Forcefully attaching socket to the CLIENT_PORT
	if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
	{
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = inet_addr(SSIPADDRESS); // Listening on SSIPADDRESS
	address.sin_port = htons(NM_PORT);

	// Bind the socket to the port
	if(bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0)
	{
		perror("bind failed");
		exit(EXIT_FAILURE);
	}

	// Listen for incoming connections
	if(listen(server_fd, 3) < 0)
	{
		perror("listen");
		exit(EXIT_FAILURE);
	}

	while(1)
	{
		if((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0)
		{
			perror("accept");
			continue;
		}

		// Execute the Command in a new thread so that SS is always listening for new connections
		ThreadArg* arg = malloc(sizeof(ThreadArg));
		arg->socket = new_socket;

		if(recv(new_socket, arg->request, sizeof(arg->request), 0) < 0)
		{
			perror("recv failed");
			close(new_socket);
			exit(EXIT_FAILURE);
		}
		printf("GOT this here %s\n", arg->request);
		pthread_t tid;
		if(pthread_create(&tid, NULL, (void* (*)(void*))executeNMRequest, arg) != 0)
		{
			perror("Failed to create thread for client request");
		}

		pthread_detach(tid); // Detach the thread
	}

	return NULL;
}

void* handleStorageServerConnectionsRecv(void* args) {
	int server_fd, new_socket, opt = 1;
	struct sockaddr_in address;
	int addrlen = sizeof(address);

	// Creating socket file descriptor
	if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
	{
		perror("socket failed");
		exit(EXIT_FAILURE);
	}

	// Forcefully attaching socket to the CLIENT_PORT
	if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
	{
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = inet_addr(SSIPADDRESS); // Listening on SSIPADDRESS
	address.sin_port = htons(SS_PORT_RECV);

	// Bind the socket to the port
	if(bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0)
	{
		perror("bind failed");
		exit(EXIT_FAILURE);
	}

	// Listen for incoming connections
	if(listen(server_fd, 3) < 0)
	{
		perror("listen");
		exit(EXIT_FAILURE);
	}

	while(1)
	{
		if((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0)
		{
			perror("accept");
			continue;
		}

		// Execute the Command in a new thread so that SS is always listening for new connections
		ThreadArg* arg = malloc(sizeof(ThreadArg));
		arg->socket = new_socket;
		memset(arg->request, '\0', sizeof(arg->request));
		if(recv(new_socket, arg->request, sizeof(arg->request), 0) < 0)
		{
			perror("recv failed");
			close(new_socket);
			exit(EXIT_FAILURE);
		}

		pthread_t tid;
		if(pthread_create(&tid, NULL, (void* (*)(void*))executeSSRequestRecv, arg) != 0)
		{
			perror("Failed to create thread for client request");
		}
	}

	return NULL;
}

void* handleStorageServerConnections(void* args)
{
	int server_fd, new_socket, opt = 1;
	struct sockaddr_in address;
	int addrlen = sizeof(address);

	// Creating socket file descriptor
	if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
	{
		perror("socket failed");
		exit(EXIT_FAILURE);
	}

	// Forcefully attaching socket to the CLIENT_PORT
	if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
	{
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = inet_addr(SSIPADDRESS); // Listening on SSIPADDRESS
	address.sin_port = htons(SS_PORT_SEND);

	// Bind the socket to the port
	if(bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0)
	{
		perror("bind failed");
		exit(EXIT_FAILURE);
	}

	// Listen for incoming connections
	if(listen(server_fd, 3) < 0)
	{
		perror("listen");
		exit(EXIT_FAILURE);
	}

	while(1)
	{
		if((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0)
		{
			perror("accept");
			continue;
		}

		// Execute the Command in a new thread so that SS is always listening for new connections
		ThreadArg* arg = malloc(sizeof(ThreadArg));
		arg->socket = new_socket;
		memset(arg->request, '\0', sizeof(arg->request));
		if(recv(new_socket, arg->request, sizeof(arg->request), 0) < 0)
		{
			perror("recv failed");
			close(new_socket);
			exit(EXIT_FAILURE);
		}

		pthread_t tid;
		if(pthread_create(&tid, NULL, (void* (*)(void*))executeSSRequest, arg) != 0)
		{
			perror("Failed to create thread for client request");
		}
	}

	return NULL;
}

int getAvailablePort() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Cannot open socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(0); // bind to any available port

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    socklen_t len = sizeof(serv_addr);
    if (getsockname(sockfd, (struct sockaddr *)&serv_addr, &len) < 0) {
        perror("getsockname failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    int port = ntohs(serv_addr.sin_port);
    close(sockfd);
    return port;
}

// The main function could set up the storage server.
int main(int argc, char* argv[])
{
	// if (argc != 6) {
	//     fprintf(stderr, "Usage: %s <NMIPADDRESS> <SSIPADDRESS> <CLIENT_PORT> <NM_PORT> <SS_PORT>\n", argv[0]);
	//     return 1;
	// }

	// // Ensure that the IP address is not too long to fit into NMIPADDRESS
	// if (strlen(argv[1]) < sizeof(NMIPADDRESS)) {
	//     strcpy(NMIPADDRESS, argv[1]);
	//     NMIPADDRESS[strlen(argv[1])] = '\0'; // Explicitly add null terminator
	// } else {
	//     fprintf(stderr, "IP address is too long.\n");
	//     return 1;
	// }

	// // Ensure that the IP address is not too long to fit into SSIPADDRESS
	// if (strlen(argv[2]) < sizeof(SSIPADDRESS)) {
	//     strcpy(SSIPADDRESS, argv[2]);
	//     SSIPADDRESS[strlen(argv[2])] = '\0'; // Explicitly add null terminator
	// } else {
	//     fprintf(stderr, "IP address is too long.\n");
	//     return 1;
	// }

	// CLIENT_PORT = atoi(argv[3]);
	// NM_PORT = atoi(argv[4]);
	// SS_PORT = atoi(argv[5]);


	// Set 3 random ports that are open
	CLIENT_PORT = getAvailablePort();
	NM_PORT = getAvailablePort();
	SS_PORT_SEND = getAvailablePort();
	SS_PORT_RECV = getAvailablePort();



	printf("Storage Server\n");
	initializeStorageServer();
	printf("%d \n This ", ss.numPaths);
	// Report to the naming server
	reportToNamingServer(&ss);

	pthread_t thread1, thread2, thread3, thread4;

	// Create thread for handling client connections
	if(pthread_create(&thread1, NULL, handleClientConnections, NULL) != 0)
	{
		perror("Failed to create client connections thread");
		return 1;
	}

	// Create thread for handling Naming Server connections
	if(pthread_create(&thread2, NULL, handleNamingServerConnections, NULL) != 0)
	{
		perror("Failed to create Naming Server connections thread");
		return 1;
	}

	// Create thread for handling Naming Server connections
	if(pthread_create(&thread3, NULL, handleStorageServerConnections, NULL) != 0)
	{
		perror("Failed to create Naming Server connections thread");
		return 1;
	}

	// Create thread for handling Naming Server connections
	if(pthread_create(&thread4, NULL, handleStorageServerConnectionsRecv, NULL) != 0)
	{
		perror("Failed to create Naming Server connections thread");
		return 1;
	}

	// Wait for threads to finish (optional based on your design)
	pthread_join(thread1, NULL);
	pthread_join(thread2, NULL);
	pthread_join(thread3, NULL);
	pthread_join(thread4, NULL);

	// Further code to accept connections and handle requests.

	return 0;
}
