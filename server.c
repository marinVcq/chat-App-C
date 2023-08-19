#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define WINVER WindowsXP

#include <windows.h>
#include <winsock2.h>
#include <winsock.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdint.h>
#include <process.h>

#if (_WIN32_WINNT >= 0x0501)
void WSAAPI freeaddrinfo (struct addrinfo*);
int WSAAPI getaddrinfo (const char*,const char*,const struct addrinfo*,
                struct addrinfo**);
int WSAAPI getnameinfo(const struct sockaddr*,socklen_t,char*,DWORD,
               char*,DWORD,int);
#else
/* FIXME: Need WS protocol-independent API helpers.  */
#endif

#define DEFAULT_PORT "12345"
#define DEFAULT_BUFLEN 1024
#define MAX_CLIENTS 10
unsigned int WINAPI ClientThread(LPVOID lpParam);

struct clientInfo{
    SOCKET socket;
    char username[DEFAULT_BUFLEN];
};

WORD wVersionRequested; 
WSADATA wsaData;
SOCKET clientSockets[MAX_CLIENTS];
CRITICAL_SECTION csClients;

struct addrinfo *result = NULL;
struct addrinfo *ptr = NULL;
struct addrinfo hints;
struct clientInfo clientList[MAX_CLIENTS];

char recvbuf[DEFAULT_BUFLEN];
int iResult, iSendResult;
int recvbuflen = DEFAULT_BUFLEN;
int numClients = 0;

int main(){

    wVersionRequested = MAKEWORD(2,2);
    iResult = WSAStartup(wVersionRequested, &wsaData);
    SOCKET ListenSocket = INVALID_SOCKET;
    SOCKET NewClientSocket = INVALID_SOCKET;

    InitializeCriticalSection(&csClients);

    // Error handling
    if(iResult != 0){
        printf("WSAStartup Failed:\n%d\n", iResult);
        return 1;
    }

    if(LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2){
        printf("Winsock DLL not find, Check the requested version\n");
        WSACleanup();
        return 1;
    }

    // 1. Initialization
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);

    if(iResult != 0){
        printf("getaddrinfo() Failed:\n %d\n", iResult);
        WSACleanup();
        return 1;
    }

    // 2. Create Socket
    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if(ListenSocket == INVALID_SOCKET){
        printf("Error at Socket(): %d", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    // Set Socket Option
    int enable = 1;
    if(setsockopt(ListenSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&enable, sizeof(enable)) == SOCKET_ERROR){
        fprintf(stderr, "setsockopt(SO_REUSEADDR) failed.\n");
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    // 3. Bind Socket
    iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if(iResult == SOCKET_ERROR){

        int error = WSAGetLastError();
        switch(error) {
            case 10013:
                fprintf(stderr, "Bind failed due to permission denied.\n");
                break;
            case 10048:
                fprintf(stderr, "Bind failed due address already in use\n");
                break;
            default:
                fprintf(stderr, "Bind failed with error: %d\n", error);
        }

        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }else{
        freeaddrinfo(result);
    }

    // 4. Listening for incomming client connection
    if(listen(ListenSocket, SOMAXCONN) == SOCKET_ERROR){

        int error = WSAGetLastError();
        switch(error) {
            default:
                fprintf(stderr, "Listen failed with error: %d\n", error);
        }
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    while(1){
        // 5. Accepting a client connection
        NewClientSocket = accept(ListenSocket, NULL, NULL);

        if(NewClientSocket == INVALID_SOCKET){
            int error = WSAGetLastError();
            switch(error) {
                default:
                    fprintf(stderr, "Accept failed with error: %d\n", error);
            }
            continue;
        }else{
            EnterCriticalSection(&csClients);

            if(numClients < MAX_CLIENTS){
                
                clientList[numClients].socket = NewClientSocket;
                memset(clientList[numClients].username,0,sizeof(clientList[numClients].username));

                // Create a thread to handle futur connection
                // Note: _beginThreadex seems more accurate than CreateThread
                HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, ClientThread, &clientList[numClients], 0, NULL);
                // HANDLE hThread = CreateThread(NULL, 0, ClientThread, &clientSockets[numClients], 0, NULL);
                
                if(hThread == NULL){
                    printf("Error creating thread\n");
                }else{
                    CloseHandle(hThread);
                    numClients++;
                }
            }else{
                printf("Max number of clients reached\n");
                closesocket(NewClientSocket);
            }
            LeaveCriticalSection(&csClients);
        }
    }

    // 7. Ending connection
    iResult = shutdown(NewClientSocket, SD_SEND);
    if(iResult == SOCKET_ERROR){
        printf("shutdown Failed: %d\n", WSAGetLastError());
        closesocket(NewClientSocket);
        WSACleanup();
        return 1;
    }

    // Delete critical section
    DeleteCriticalSection(&csClients);
    return 0;
}

unsigned int WINAPI ClientThread(LPVOID lpParam) {
    struct clientInfo* client = (struct clientInfo*)lpParam;
    SOCKET ClientSocket = *((SOCKET*)lpParam);

    char recvbuf[DEFAULT_BUFLEN];
    int iResult, iSendResult;
    int recvbuflen = DEFAULT_BUFLEN;
    extern int numClients;

    // Check for username availability
    int usernameTaken = 1;
    while (usernameTaken) {
        iResult = recv(ClientSocket, client->username, DEFAULT_BUFLEN, 0);
        if (iResult > 0) {
            int foundTaken = 0;
            for (int i = 0; i < numClients; i++) {
                if (clientList[i].socket != ClientSocket &&
                    strcmp(clientList[i].username, client->username) == 0) {
                    foundTaken = 1;
                    break; // No need to continue checking once taken username is found
                }
            }

            char availabilityMsg[10];
            if (foundTaken) {
                strcpy(availabilityMsg, "taken");
            } else {
                strcpy(availabilityMsg, "available");
                printf("Client connected with username: %s\n", client->username);
                usernameTaken = 0;
            }

            iSendResult = send(ClientSocket, availabilityMsg, 10, 0);
            if (iSendResult == SOCKET_ERROR) {
                printf("Send username availability failed: %d\n", WSAGetLastError());
                break;
            }
        } else if (iResult == 0) {
            printf("Client disconnected while trying to authenticate.\n");
            break;
        } else {
            printf("recv Failed (Username availability): %d\n", WSAGetLastError());
            break;
        }
    }
    
    do {
        iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);

        if (iResult > 0) {
            printf("Bytes received: %d\n", iResult);
            printf("Client: %s\n", recvbuf);

            // Broadcast the received message to all other clients
            for (int j = 0; j < numClients; j++) {
                if (clientList[j].socket != ClientSocket) {

                    char broadcastMessage[DEFAULT_BUFLEN + DEFAULT_BUFLEN];

                    // Remove newline character from client->username
                    char* newlinePos = strchr(client->username, '\n');
                    if (newlinePos != NULL) {
                        *newlinePos = '\0';
                    }

                    // Concatenate username and recvbuf without any separation
                    strcpy(broadcastMessage, client->username);
                    strcat(broadcastMessage, ": ");
                    strcat(broadcastMessage, recvbuf);


                    // snprintf(broadcastMessage, sizeof(broadcastMessage), "%s:%s", client->username, recvbuf);
                    // // Remove newline character from the sender's username
                    // char* newlinePos = strchr(broadcastMessage, '\n');
                    // if (newlinePos != NULL) {
                    //     *newlinePos = '\0';
                    // }
                    printf("broadcast: %s\n", broadcastMessage);

                    iSendResult = send(clientList[j].socket, broadcastMessage,strlen(broadcastMessage), 0);
                    if (iSendResult == SOCKET_ERROR) {
                        printf("Send Failed: %d\n", WSAGetLastError());
                    } else {
                        printf("Bytes sent: %d\n", iSendResult);
                    }
                }
            }
        } else if (iResult == 0) {
            printf("Connection closing...\n");
        } else {
            printf("recv Failed (ClientThread): %d\n", WSAGetLastError());
        }
    } while (iResult > 0);

    // Remove the client socket from the list
    EnterCriticalSection(&csClients);
    for (int i = 0; i < numClients; i++) {
        if (clientList[i].socket == ClientSocket) {
            for (int j = i; j < numClients - 1; j++) {
                clientList[j].socket = clientList[j + 1].socket;
            }
            numClients--;
            break;
        }
    }
    LeaveCriticalSection(&csClients);
    // Close the client socket and exit the thread
    DeleteCriticalSection(&csClients);
    closesocket(ClientSocket);
    return 0;
}
