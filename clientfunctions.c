#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <time.h>
#define NAMING_SERVER_PORT 8001
#define IP_ADDRESS "127.0.0.1"

#define TIMEOUT 3

time_t start;

char request[1000]; // global variable to take input of from client

int waitforAck(int clientSocket)
{
    time_t end;
    char buffer[1024];
    memset(buffer, '\0', sizeof(buffer));
    ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
    if(bytesRead == -1)
    {
        perror("recv failed");
    }
    else if(bytesRead == 0)
    {
        printf("Connection closed by peer\n");
    }
    else
    {
        time(&end);
        double timeTaken = end - start;
        // printf("Time taken: %lf\n", timeTaken);
        if (timeTaken > TIMEOUT)
        {
            printf("Timeout\n");
            return 0;
        }
        else
        {
            printf("Ack received\n");
            return 1;
        }
    }
}

// read function when client requests to read a file
void clientRead(int clientSocket)
{

    // send the request to the Naming Server
    send(clientSocket, request, strlen(request), 0);

    time(&start);
    int ack = waitforAck(clientSocket);
    if (ack == 0)
    {
        printf("Acknowledge not received\n");
        return;
    }

    // receive the response from the Naming Server
    char response[10000];
    memset(response, '\0', sizeof(response));
    recv(clientSocket, response, sizeof(response), 0);

    if (strcmp(response, "File not Found") == 0)
    {
        printf("The file doesn't exist\n");
    }
    else
    {

        // tokenising the response into a token array
        char *tokenArrayresponse[10];
        char *tokenresponse = strtok(response, " ");
        int j = 0;
        while (tokenresponse != NULL)
        {
            tokenArrayresponse[j++] = tokenresponse;
            tokenresponse = strtok(NULL, " ");
        }

        // using the response, client tries to connect to the Storage Server TCP socket
        int storageServerSocket = socket(AF_INET, SOCK_STREAM, 0);

        struct sockaddr_in storageServerAddress;
        storageServerAddress.sin_family = AF_INET;
        storageServerAddress.sin_port = htons(atoi(tokenArrayresponse[1]));
        storageServerAddress.sin_addr.s_addr = inet_addr(tokenArrayresponse[0]);

        int connectionStatus = connect(storageServerSocket, (struct sockaddr *)&storageServerAddress, sizeof(storageServerAddress));
        if (connectionStatus < 0)
        {
            printf("Error in connection establishment\n");
            exit(1);
        }

        // client sends request to Storage Server
        send(storageServerSocket, request, strlen(request), 0);

        // client receives response from Storage Server
        char responseStorage[10000];
        memset(responseStorage, '\0', sizeof(responseStorage));
        recv(storageServerSocket, responseStorage, sizeof(responseStorage), 0);
        if (strcmp(responseStorage, "Read error") == 0 || strcmp(responseStorage, "No content") == 0 || strcmp(responseStorage, "File not Found") == 0)
        {
            printf("The following error was encountered: \n");
            printf("%s\n", responseStorage);
        }
        else
        {

            printf("File is read as :\n");
            printf("%s\n", responseStorage);
        }

        // client closes connection to Storage Server
        close(storageServerSocket);
    }
}

// write function when client requests to write a file
void clientWrite(int clientSocket)
{
    // send request to Naming Server
    send(clientSocket, request, strlen(request), 0);

    time(&start);
    int ack = waitforAck(clientSocket);
    if (ack == 0)
    {
        printf("Acknowledge not received\n");
        return;
    }

    // receive response from Naming Server
    char response[10000];
    memset(response, '\0', sizeof(response));
    recv(clientSocket, response, sizeof(response), 0);
    printf("SEnten\n");
    if (strcmp(response, "File not Found") == 0)
    {
        printf("The file doesn't exist\n");
    }
    else
    {

        // tokenising the response into a token array
        char *tokenArrayresponse[10];
        char *tokenresponse = strtok(response, " ");
        int j = 0;
        while (tokenresponse != NULL)
        {
            tokenArrayresponse[j++] = tokenresponse;
            tokenresponse = strtok(NULL, " ");
        }

        // using the response, client tries to connect to the Storage Server TCP socket
        int storageServerSocket = socket(AF_INET, SOCK_STREAM, 0);

        struct sockaddr_in storageServerAddress;
        storageServerAddress.sin_family = AF_INET;
        storageServerAddress.sin_port = htons(atoi(tokenArrayresponse[1]));
        storageServerAddress.sin_addr.s_addr = inet_addr(tokenArrayresponse[0]);

        int connectionStatus = connect(storageServerSocket, (struct sockaddr *)&storageServerAddress, sizeof(storageServerAddress));
        if (connectionStatus < 0)
        {
            printf("Error in connection establishment\n");
            exit(1);
        }

        // client sends request to Storage Server
        send(storageServerSocket, request, strlen(request), 0);

        // client receives response from Storage Server
        char responseStorage[10000];
        memset(responseStorage, '\0', sizeof(responseStorage));
        recv(storageServerSocket, responseStorage, sizeof(responseStorage), 0);

        if (strcmp(responseStorage, "Error opening file for writing") == 0 || strcmp(responseStorage, "Write error") == 0 || strcmp(responseStorage, "File not Found") == 0)
        {
            printf("The following error was encountered: \n");
            printf("%s\n", responseStorage);
        }
        else
        {

            // printf("File has been written :\n");
            printf("%s\n", responseStorage);
        }

        // client closes connection to Storage Server
        close(storageServerSocket);
    }
}

// getsize function when client requests to get the size of a file
void clientGetSize(int clientSocket)
{
    // send request to Naming Server
    // strcpy(request, "COPY ./src/hello ./src/jainit\n");
    send(clientSocket, request, strlen(request), 0);

    time(&start);
    int ack = waitforAck(clientSocket);
    if (ack == 0)
    {
        printf("Acknowledge not received\n");
        return;
    }

    // receive response from Naming Server
    char response[10000];
    memset(response, '\0', sizeof(response));
    recv(clientSocket, response, sizeof(response), 0);

    if (strcmp(response, "File not Found") == 0)
    {
        printf("The file doesn't exist\n");
    }
    else
    {

        // tokenising the response into a token array
        char *tokenArrayresponse[10];
        char *tokenresponse = strtok(response, " ");
        int j = 0;
        while (tokenresponse != NULL)
        {
            tokenArrayresponse[j++] = tokenresponse;
            tokenresponse = strtok(NULL, " ");
        }

        // using the response, client tries to connect to the Storage Server TCP socket
        int storageServerSocket = socket(AF_INET, SOCK_STREAM, 0);

        struct sockaddr_in storageServerAddress;
        storageServerAddress.sin_family = AF_INET;
        storageServerAddress.sin_port = htons(atoi(tokenArrayresponse[1]));
        storageServerAddress.sin_addr.s_addr = inet_addr(tokenArrayresponse[0]);

        int connectionStatus = connect(storageServerSocket, (struct sockaddr *)&storageServerAddress, sizeof(storageServerAddress));
        if (connectionStatus < 0)
        {
            printf("Error in connection establishment\n");
            exit(1);
        }

        // client sends request to Storage Server
        send(storageServerSocket, request, strlen(request), 0);

        // client receives response from Storage Server
        char responseStorage[10000];
        memset(responseStorage, '\0', sizeof(responseStorage));
        recv(storageServerSocket, responseStorage, sizeof(responseStorage), 0);

        if (strcmp(responseStorage, "Error getting file size") == 0 || strcmp(responseStorage, "File not Found") == 0)
        {

            printf("The following error was encountered: \n");
            printf("%s\n", responseStorage);
        }
        else
        {

            // printf("File is read as :\n");
            printf("%s\n", responseStorage);
        }
        // client closes connection to Storage Server
        close(storageServerSocket);
    }
}

// creat function to send request to Naming Server to create a file
void clientCreate(int clientSocket)
{
    // send request to Naming Server
    send(clientSocket, request, strlen(request), 0);

    time(&start);
    int ack = waitforAck(clientSocket);
    if (ack == 0)
    {
        printf("Acknowledge not received\n");
        return;
    }

    char response[10000];
    memset(response, '\0',sizeof(response));
    recv(clientSocket, response, sizeof(response), 0);

    int index_case;
    index_case = atoi(response);

    if (index_case == 2)
    {
        printf("Error creating directory\n");
    }
    else if (index_case == 1)
    {
        printf("Directory created\n");
    }
    else if (index_case == 3)
    {
        printf("Error creating file\n");
    }
    else if (index_case == 4)
    {
        printf("File created\n");
    }
}

// delete function to send request to Naming Server to delete a file
void clientDelete(int clientSocket)
{
    // send request to Naming Server
    send(clientSocket, request, strlen(request), 0);

    time(&start);
    int ack = waitforAck(clientSocket);
    if (ack == 0)
    {
        printf("Acknowledge not received\n");
        return;
    }

    char response[10000];
    memset(response, '\0',sizeof(response));
    recv(clientSocket, response, sizeof(response), 0);

    int index_case;
    index_case = atoi(response);

    if (index_case == 1)
    {
        printf("Error deleting directory\n");
    }
    else if (index_case == 2)
    {
        printf("Directory deleted\n");
    }
    else if (index_case == 3)
    {
        printf("File deleted\n");
    }
    else if (index_case == 4)
    {
        printf("Error deleting file\n");
    }
    else if (index_case == 5)
    {
        printf("File not Found\n");
    }
}

// copy function to send request to Naming server to copy
void clientCopy(int clientSocket)
{
    // send request to Naming Server
    // strcpy(request, "COPY ./src/hello ./src/jainit\n");
    send(clientSocket, request, strlen(request), 0);

    time(&start);
    int ack = waitforAck(clientSocket);
    if (ack == 0)
    {
        printf("Acknowledge not received\n");
        return;
    }

    char response[10000];
    memset(response, '\0',sizeof(response));
    recv(clientSocket, response, sizeof(response), 0);

    int index_case;
    index_case = atoi(response);
    printf("index_case = %d\n", index_case);
    switch (index_case)
    {
    case 2:
        printf("No such folder in destination\n");
        break;

    case 3:
        printf("Error in Identifying FOLDER \n");
        break;

    case 4:
        printf("Error in copying the directory \n");
        break;
    case 5:
        printf("folder conflict \n");
        break;

    case 6:
        printf("Error in copying the file \n");
        break;
    case 13:
        printf("Error in opening the file in directory \n");
        break;
    case 7:
        printf("Error in copying the file\n");
        break;

    case 8:
        printf("Cannot open file\n");
        break;
    case 11:
        printf("Successfully done\n");
        break;
    default:
        break;
    }
}

// ls function to send request to naming server to list all accessible paths
void clientListAll(int clientSocket)
{
    // Send request to Naming Server
    send(clientSocket, request, strlen(request), 0);

    time(&start);
    int ack = waitforAck(clientSocket);
    if (ack == 0)
    {
        printf("Acknowledge not received\n");
        return;
    }

    char response[40960];
    memset(response, '\0', sizeof(response));
    recv(clientSocket, response, sizeof(response), 0);

    int i = 0;
    while (response[i] != '\0')
    {
        // Check for a new server block
        if (response[i] == '*')
        {
            i++; // Move past the '*'
            if (response[i] == '\0')
                break;
            // Extract and print the server number
            printf("\nStorage Server: ");
            while (response[i] != '$' && response[i] != '\0')
            {
                printf("%c", response[i]);
                i++;
            }
        }

        // Check for a new path
        if (response[i] == '$')
        {
            i++; // Move past the '$'

            // Extract and print the path
            printf("\nAccessible path: ");
            while (response[i] != '$' && response[i] != '*' && response[i] != '\0')
            {
                printf("%c", response[i]);
                i++;
            }
        }
    }
    printf("\n"); // New line at the end
}

void removeWhitespace(char *inputString)
{
    // Initialize indices for reading and writing in the string
    int readIndex = 0;
    int writeIndex = 0;

    // Iterate through the string
    while (inputString[readIndex] != '\0')
    {
        // If the current character is not a whitespace character, copy it
        if (!isspace(inputString[readIndex]))
        {
            inputString[writeIndex] = inputString[readIndex];
            writeIndex++;
        }

        // Move to the next character in the string
        readIndex++;
    }

    // Null-terminate the new string
    inputString[writeIndex] = '\0';
}

// Function to remove leading and trailing whitespaces
void strip(char *str)
{
    int start = 0, end = strlen(str) - 1;

    // Remove leading whitespaces
    while (str[start] && isspace((unsigned char)str[start]))
    {
        start++;
    }

    // Remove trailing whitespaces
    while (end >= start && isspace((unsigned char)str[end]))
    {
        end--;
    }

    // Shift all characters back to the start of the string
    for (int i = 0; start <= end; i++, start++)
    {
        str[i] = str[start];
    }
    str[end + 1] = '\0'; // Null-terminate the string
}

int main()
{
    // client tries to connect to Naming Server TCP socket
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0)
    {
        printf("Error in client socket creation\n");
        exit(1);
    }

    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(NAMING_SERVER_PORT);
    serverAddress.sin_addr.s_addr = inet_addr(IP_ADDRESS);

    int connectionStatus = connect(clientSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress));
    if (connectionStatus < 0)
    {
        printf("Error in connection establishment\n");
        exit(1);
    }

    // client sends request to Naming Server
    while (1)
    {
        printf("Enter request: ");
        memset(request, '\0', sizeof(request));
        fgets(request, sizeof(request), stdin);
        strip(request); // Strip the input request

        // Use strncmp to compare the first few characters of the command
        if (strncmp(request, "READ", 4) == 0)
        {
            clientRead(clientSocket);
        }
        else if (strncmp(request, "WRITE", 5) == 0)
        {
            clientWrite(clientSocket);
        }
        else if (strncmp(request, "GETSIZE", 7) == 0)
        {
            clientGetSize(clientSocket);
        }
        else if (strncmp(request, "CREATE", 6) == 0)
        {
            clientCreate(clientSocket);
        }
        else if (strncmp(request, "DELETE", 6) == 0)
        {
            clientDelete(clientSocket);
        }
        else if (strncmp(request, "COPY", 4) == 0)
        {
            clientCopy(clientSocket);
        }
        else if (strncmp(request, "LISTALL", 7) == 0)
        {
            clientListAll(clientSocket);
        }
        else
        {
            printf("Invalid request\n");
        }

        fflush(stdin);
        memset(request, '\0', sizeof(request));
    }

    return 0;
}
