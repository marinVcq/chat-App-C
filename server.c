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
WORD wVersionRequested; 
WSADATA wsaData;
// DWORD WINAPI ClientThread(LPVOID lpParam);
SOCKET clientSockets[MAX_CLIENTS];

CRITICAL_SECTION csNumClients;
CRITICAL_SECTION csClientSockets;


struct addrinfo *result = NULL;
struct addrinfo *ptr = NULL;
struct addrinfo hints;

char recvbuf[DEFAULT_BUFLEN];
int iResult, iSendResult;
int recvbuflen = DEFAULT_BUFLEN;
int numClients = 0;

int main(){

    wVersionRequested = MAKEWORD(2,2);
    iResult = WSAStartup(wVersionRequested, &wsaData);
    SOCKET ListenSocket = INVALID_SOCKET;
    SOCKET ClientSocket = INVALID_SOCKET;

    InitializeCriticalSection(&csNumClients);
    InitializeCriticalSection(&csClientSockets);

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
        SOCKET NewClientSocket = accept(ListenSocket, NULL, NULL);

        if(NewClientSocket == INVALID_SOCKET){
            int error = WSAGetLastError();
            switch(error) {
                default:
                    fprintf(stderr, "Accept failed with error: %d\n", error);
            }
            closesocket(ListenSocket);
            WSACleanup();
            return 1;
        }else{
            EnterCriticalSection(&csNumClients);
            EnterCriticalSection(&csClientSockets);

            if(numClients < MAX_CLIENTS){
                clientSockets[numClients] = NewClientSocket;

                // Create a thread to handle futur connection
                // Note: _beginThreadex seems more accurate than CreateThread
                HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, ClientThread, &clientSockets[numClients], 0, NULL);
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
            LeaveCriticalSection(&csNumClients);
            LeaveCriticalSection(&csClientSockets);
        }
    }

    // 7. Ending connection
    iResult = shutdown(ClientSocket, SD_SEND);
    if(iResult == SOCKET_ERROR){
        printf("shutdown Failed: %d\n", WSAGetLastError());
        closesocket(ClientSocket);
        WSACleanup();
        return 1;
    }

    // Delete critical section
    DeleteCriticalSection(&csNumClients);
    DeleteCriticalSection(&csClientSockets);
    return 0;
}

unsigned int WINAPI ClientThread(LPVOID lpParam) {
    SOCKET ClientSocket = *((SOCKET*)lpParam);

    char recvbuf[DEFAULT_BUFLEN];
    int iResult, iSendResult;
    int recvbuflen = DEFAULT_BUFLEN;

    extern SOCKET clientSockets[MAX_CLIENTS]; // Declare the array of client sockets
    extern int numClients; // Declare the variable to track the number of clients

    do {
        iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);

        if (iResult > 0) {
            printf("Bytes received: %d\n", iResult);
            printf("Client: %s\n", recvbuf);

            // Broadcast the received message to all other clients
            for (int j = 0; j < numClients; j++) {
                if (clientSockets[j] != ClientSocket) {
                    iSendResult = send(clientSockets[j], recvbuf, iResult, 0);
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
            printf("recv Failed: %d\n", WSAGetLastError());
        }
    } while (iResult > 0);

    // Remove the client socket from the list
    EnterCriticalSection(&csNumClients);
    for (int i = 0; i < numClients; i++) {
        if (clientSockets[i] == ClientSocket) {
            for (int j = i; j < numClients - 1; j++) {
                clientSockets[j] = clientSockets[j + 1];
            }
            numClients--;
            break;
        }
    }
    LeaveCriticalSection(&csNumClients);
    // Close the client socket and exit the thread
    DeleteCriticalSection(&csNumClients);
    closesocket(ClientSocket);
    return 0;
}
