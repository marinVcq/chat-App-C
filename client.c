#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>

#define DEFAULT_PORT "12345"
#define DEFAULT_BUFLEN 1024

int main() {
    WSADATA wsaData;
    SOCKET ConnectSocket = INVALID_SOCKET;
    struct sockaddr_in serverAddr;
    int iResult;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed: %d\n", iResult);
        return 1;
    }

    // Create a socket
    ConnectSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ConnectSocket == INVALID_SOCKET) {
        printf("Socket creation failed: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    // Setup the server address struct
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(atoi(DEFAULT_PORT));
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Connect to the server
    iResult = connect(ConnectSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    if (iResult == SOCKET_ERROR) {
        printf("Connection failed: %d\n", WSAGetLastError());
        closesocket(ConnectSocket);
        WSACleanup();
        return 1;
    }

    char sendbuf[DEFAULT_BUFLEN];
    char recvbuf[DEFAULT_BUFLEN];

    // Enter main loop to send/receive messages
    while (1) {
        printf("Enter message: ");
        fgets(sendbuf, DEFAULT_BUFLEN, stdin);

        // Send data to the server
        iResult = send(ConnectSocket, sendbuf, strlen(sendbuf), 0);
        if (iResult == SOCKET_ERROR) {
            printf("Send failed: %d\n", WSAGetLastError());
            closesocket(ConnectSocket);
            WSACleanup();
            return 1;
        }

        // Receive data from the server
        iResult = recv(ConnectSocket, recvbuf, DEFAULT_BUFLEN, 0);
        if (iResult > 0) {
            recvbuf[iResult] = '\0';
            printf("Server: %s\n", recvbuf);
        } else if (iResult == 0) {
            printf("Connection closed by server.\n");
            break;
        } else {
            printf("Receive failed: %d\n", WSAGetLastError());
            closesocket(ConnectSocket);
            WSACleanup();
            return 1;
        }
    }

    // Cleanup and close the socket
    closesocket(ConnectSocket);
    WSACleanup();

    return 0;
}
