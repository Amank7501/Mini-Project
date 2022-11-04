#include <stdio.h>      // Import for `printf` & `perror` functions
#include <errno.h>      // Import for `errno` variable
#include <fcntl.h>      // Import for `fcntl` functions
#include <unistd.h>     // Import for `fork`, `fcntl`, `read`, `write`, `lseek, `_exit` functions
#include <sys/types.h>  // Import for `socket`, `bind`, `listen`, `connect`, `fork`, `lseek` functions
#include <sys/socket.h> // Import for `socket`, `bind`, `listen`, `connect` functions
#include <netinet/ip.h> // Import for `sockaddr_in` stucture
#include <string.h>     // Import for string functions

void connection_handler(int sockFD); // Handles the read & write operations to the server

void main()
{
    int socketFD, connStat;
    struct sockaddr_in serverAddress;
    struct sockaddr server;

    socketFD = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFD == -1)//Error in creating server socket
    {
        perror("Error in creating server socket!");
        _exit(0);
    }

    serverAddress.sin_family = AF_INET;                // IPv4 domain
    serverAddress.sin_port = htons(8081);              // Server will listen to 8080 port
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY); // Binds the socket to all interfaces

    connStat = connect(socketFD, (struct sockaddr *)&serverAddress, sizeof(serverAddress));
    if (connStat == -1)//Error in connecting to server
    {
        perror("Error in connecting to server!");
        close(socketFD);
        _exit(0);
    }

    connection_handler(socketFD);//calling connection handler

    close(socketFD);//close socket file descriptor
}
// Handles the read & write operations w the server
void connection_handler(int sockFD)
{
    char readBuff[1000], writeBuff[1000]; // A buffer used for reading from / writting to the server
    ssize_t readBytes, writeBytes;            // Number of bytes read from / written to the socket

    char tempBuff[1000];

    do
    {
        bzero(readBuff, sizeof(readBuff)); // Empty the read buffer
        bzero(tempBuff, sizeof(tempBuff));
        readBytes = read(sockFD, readBuff, sizeof(readBuff));
        if (readBytes == -1)//Error in reading from client socket
            perror("Error in reading from client socket!");
        else if (readBytes == 0)
            printf("No error received from server! Closing the connection to the server now!\n");
        else if (strchr(readBuff, '^') != NULL)
        {
            // Skip read from client
            strncpy(tempBuff, readBuff, strlen(readBuff) - 1);
            printf("%s\n", tempBuff);
            writeBytes = write(sockFD, "^", strlen("^"));
            if (writeBytes == -1)//Error in writing to client socket
            {
                perror("Error in writing to client socket!");
                break;
            }
        }
        else if (strchr(readBuff, '$') != NULL)
        {
            // Server sent an error message and is now closing it's end of the connection
            strncpy(tempBuff, readBuff, strlen(readBuff) - 2);
            printf("%s\n", tempBuff);
            printf("Closing the connection to the server now!\n");
            break;
        }
        else
        {
            bzero(writeBuff, sizeof(writeBuff)); // Empty the write buffer

            if (strchr(readBuff, '#') != NULL)
                strcpy(writeBuff, getpass(readBuff));
            else
            {
                printf("%s\n", readBuff);
                scanf("%[^\n]%*c", writeBuff); // Take user input!
            }

            writeBytes = write(sockFD, writeBuff, strlen(writeBuff));
            if (writeBytes == -1)//Error in writing to client socket
            {
                perror("Error in writing to client socket!");
                printf("Closing the connection to the server now!\n");
                break;
            }
        }
    } while (readBytes > 0);

    close(sockFD);//close client socket file descriptor
}
