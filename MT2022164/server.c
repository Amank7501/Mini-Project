#include <stdio.h> // Import for `printf` & `perror` functions
#include <errno.h> // Import for `errno` variable

#include <fcntl.h>      // Import for `fcntl` functions
#include <unistd.h>     // Import for `fork`, `fcntl`, `read`, `write`, `lseek, `_exit` functions
#include <sys/types.h>  // Import for `socket`, `bind`, `listen`, `accept`, `fork`, `lseek` functions
#include <sys/socket.h> // Import for `socket`, `bind`, `listen`, `accept` functions
#include <netinet/ip.h> // Import for `sockaddr_in` stucture

#include <string.h>  // Import for string functions
#include <stdbool.h> // Import for `bool` data type
#include <stdlib.h>  // Import for `atoi` function

#include "./All-functions/server-constants.h"
#include "./All-functions/admin.h"
#include "./All-functions/customer.h"

void connection_handler(int connFD); // Handles the communication with the client

void main()
{
    int socketFD, socketBindStat, socketListenStat, connFD;
    struct sockaddr_in serverAdd, clientAdd;

    socketFD = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFD == -1)//Error in creating server socket
    {
        perror("Error in creating server socket!");
        _exit(0);
    }

    serverAdd.sin_family = AF_INET;                // IPv4 domain
    serverAdd.sin_port = htons(8081);              // Server will listen to 8080 port
    serverAdd.sin_addr.s_addr = htonl(INADDR_ANY); // Binds the socket to all interfaces

    socketBindStat = bind(socketFD, (struct sockaddr *)&serverAdd, sizeof(serverAdd));//binding
    if (socketBindStat == -1)//Error in binding to server socket
    {
        perror("Error in binding to server socket!");
        _exit(0);
    }

    socketListenStat = listen(socketFD, 10);
    if (socketListenStat == -1)//Error in listening for connections on the server socket
    {
        perror("Error in listening for connections on the server socket!");
        close(socketFD);//socket file descriptor
        _exit(0);
    }

    int clientSize;
    while (1)
    {
        clientSize = (int)sizeof(clientAdd);//client size
        connFD = accept(socketFD, (struct sockaddr *)&clientAdd, &clientSize);//accept()
        if (connFD == -1)//Error in connecting to client
        {
            perror("Error in connecting to client!");
            close(socketFD);
        }
        else
        {
            if (!fork())
            {
                // Child will enter this branch
                connection_handler(connFD);
                close(connFD);//close connection file descriptor
                _exit(0);
            }
        }
    }

    close(socketFD);//close socket file descriptor
}

void connection_handler(int connectionFD)
{
    printf("Client has connected to the server!\n");

    char readBuff[1000], writeBuff[1000];
    ssize_t readBytes, writeBytes;
    int userChoice;

    writeBytes = write(connectionFD, INITIAL_PROMPT, strlen(INITIAL_PROMPT));
    if (writeBytes == -1)//Error in sending first prompt to the user
        perror("Error in sending first prompt to the user!");
    else
    {
        bzero(readBuff, sizeof(readBuff));//empty the read buffer
        readBytes = read(connectionFD, readBuff, sizeof(readBuff));
        if (readBytes == -1)//Error in reading from client
            perror("Error in reading from client");
        else if (readBytes == 0)
            printf("No data was sent by the client");
        else
        {
            userChoice = atoi(readBuff);//user choice
            switch (userChoice)
            {
            case 1:
                // Admin
                admin_operation_handler(connectionFD);
                break;
            case 2:
                // Customer
                customer_operation_handler(connectionFD);
                break;
            default:
                // Exit
                break;
            }
        }
    }
    printf("Terminating connection to client!\n");
}
