#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <process.h>

#define DEFAULT_PORT "12345"
#define DEFAULT_BUFLEN 1024

unsigned int WINAPI ReceiveThread(LPVOID lpParam);
unsigned int WINAPI SendThread(LPVOID lpParam);

HANDLE promptMutex; 
HANDLE recvThread;
HANDLE sendThread;

int main() {
    WSADATA wsaData;
    SOCKET ConnectSocket = INVALID_SOCKET;
    struct sockaddr_in serverAddr;
    int iResult;
    promptMutex = CreateMutex(NULL, FALSE, NULL);


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

    // Authentication loop
    int authenticated = 0;
    while (!authenticated) {
        printf("Enter username: ");
        fgets(sendbuf, DEFAULT_BUFLEN, stdin);

        // Send username to the server
        iResult = send(ConnectSocket, sendbuf, strlen(sendbuf), 0);
        if (iResult == SOCKET_ERROR) {
            printf("Send username failed: %d\n", WSAGetLastError());
            closesocket(ConnectSocket);
            WSACleanup();
            return 1;
        }

        // Receive username availability response
        iResult = recv(ConnectSocket, recvbuf, 10, 0);
        if (iResult > 0) {
            recvbuf[iResult] = '\0';
            if (strcmp(recvbuf, "available") == 0) {
                authenticated = 1;
                printf("Username available. You are now authenticated.\n");
                break;
            } else {
                printf("Username is taken. Please choose another.\n");
            }
        } else {
            printf("Receive username availability failed: %d\n", WSAGetLastError());
            closesocket(ConnectSocket);
            WSACleanup();
            return 1;
        }
    }


    recvThread = (HANDLE)_beginthreadex(NULL, 0, ReceiveThread, &ConnectSocket, 0, NULL);
    if (recvThread == NULL) {
        printf("Error creating receive thread\n");
        closesocket(ConnectSocket);
        WSACleanup();
        return 1;
    }

    // Create a thread to handle sending messages
    sendThread = (HANDLE)_beginthreadex(NULL, 0, SendThread, &ConnectSocket, 0, NULL);
    if (sendThread == NULL) {
        printf("Error creating send thread\n");
        closesocket(ConnectSocket);
        WSACleanup();
        return 1;
    }

    // Wait for the threads to finish
    WaitForSingleObject(recvThread, INFINITE);
    WaitForSingleObject(sendThread, INFINITE);

    // Cleanup and close the socket
    closesocket(ConnectSocket);
    WSACleanup();
    return 0;
}


unsigned int WINAPI ReceiveThread(LPVOID lpParam) {
    SOCKET ConnectSocket = *((SOCKET*)lpParam);
    char recvbuf[DEFAULT_BUFLEN];
    int iResult;

    while (1) {
        iResult = recv(ConnectSocket, recvbuf, DEFAULT_BUFLEN, 0);
        if (iResult > 0) {
            recvbuf[iResult] = '\0';
            printf("\n%s", recvbuf);

            // Terminate and recreate the SendThread
            TerminateThread(sendThread, 0); // Use with caution
            sendThread = (HANDLE)_beginthreadex(NULL, 0, SendThread, &ConnectSocket, 0, NULL);
            
            WaitForSingleObject(promptMutex, INFINITE);

            fflush(stdout);

            ReleaseMutex(promptMutex);

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

    return 0;
}

unsigned int WINAPI SendThread(LPVOID lpParam) {
    SOCKET ConnectSocket = *((SOCKET*)lpParam);
    char sendbuf[DEFAULT_BUFLEN];

    while (1) {
        WaitForSingleObject(promptMutex, INFINITE);
        printf("Enter message: ");
        fgets(sendbuf, DEFAULT_BUFLEN, stdin);
        ReleaseMutex(promptMutex);

        // Send data to the server
        int iResult = send(ConnectSocket, sendbuf, strlen(sendbuf), 0);
        if (iResult == SOCKET_ERROR) {
            printf("Send failed: %d\n", WSAGetLastError());
            closesocket(ConnectSocket);
            WSACleanup();
            return 1;
        }
    }

    return 0;
}


