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
#define MAX_CLIENTS_PER_ROOM 5
#define MAX_ROOMS 15
unsigned int WINAPI ClientThread(LPVOID lpParam);

struct clientInfo{
    SOCKET socket;
    char username[DEFAULT_BUFLEN];
    char room[DEFAULT_BUFLEN]; // Room ID
    int connectedToRoom;
};

struct RoomClients{
    char room[DEFAULT_BUFLEN];
    char password[DEFAULT_BUFLEN];
    struct clientInfo clients[MAX_CLIENTS_PER_ROOM];
    int numClients;
};

struct RoomClients roomList[MAX_ROOMS];


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
int numRooms = 0;

// Function
int printColorText(const char *text, const char *buffer, int colorCode){
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, colorCode);
    printf("%s %s",text, buffer);
    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    return 0;
}

int main(){

    for (int i = 0; i < MAX_ROOMS; i++) {
        roomList[i].numClients = 0;
    }

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
        memset(recvbuf, 0, sizeof(recvbuf));
    }

    // Connect to room
    while(!client->connectedToRoom){
        iResult = recv(ClientSocket, client->room, DEFAULT_BUFLEN, 0);
        if (iResult > 0) {
            int roomExists = 0;
            int passwordOK = 0;

            // check if room already exist
            for(int i=0;i<numRooms; i++){
                if(strcmp(roomList[i].room, client->room) == 0){
                    roomExists = 1;

                    char temp[] = "available";
                    send(ClientSocket, temp, sizeof(temp), 0);
                    
                    while(!passwordOK){
                        // If the room exists, ask for the password
                        char passwordPrompt[] = "Enter room password: ";
                        send(ClientSocket, passwordPrompt, sizeof(passwordPrompt), 0);

                        char receivedPassword[DEFAULT_BUFLEN];
                        recv(ClientSocket, receivedPassword, DEFAULT_BUFLEN, 0);

                        if (strcmp(receivedPassword, roomList[i].password) == 0) {
                            // Password matches, connect the client to the room
                            client->connectedToRoom = 1;
                            char serverResponse[] = "Connected";
                            send(ClientSocket, serverResponse, sizeof(serverResponse), 0);
                            passwordOK = 1;
                        } else {
                            // Password doesn't match, close the connection
                            char incorrectPasswordMsg[] = "Incorrect password";
                            send(ClientSocket, incorrectPasswordMsg, sizeof(incorrectPasswordMsg), 0);
                        }
                    }
                   break;                        
                }
            }

            if(!roomExists){
                // Create the room 
                if(numRooms < MAX_ROOMS){

                    char temp[] = "available";
                    send(ClientSocket, temp, sizeof(temp), 0);

                    // Create a password
                    char setPasswordMsg[] = "Enter a password for the new room: ";
                    send(ClientSocket, setPasswordMsg, sizeof(setPasswordMsg), 0);

                    recv(ClientSocket, roomList[numRooms].password, DEFAULT_BUFLEN, 0);

                    // Initialize the new room
                    strcpy(roomList[numRooms].room, client->room);
                    roomList[numRooms].numClients = 0;
                    numRooms++;

                    // Connect the client to the room
                    client->connectedToRoom = 1;

                }else{
                    // Notify the client that the maximum number of rooms is reached
                    char maxRoomsMsg[] = "Max rooms reached";
                    send(ClientSocket, maxRoomsMsg, sizeof(maxRoomsMsg), 0);
                    closesocket(ClientSocket);
                    return 0;                   
                }
            }


        }else if (iResult == 0) {
            printf("Client disconnected while trying to join.\n");
            break;
        } else {
            printf("recv Failed (room availability): %d\n", WSAGetLastError());
            break;
        }
    }
    
    do {
        iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);

        if (iResult > 0) {
            recvbuf[iResult] = '\0'; // Ensure proper null-termination
            // printf("Received Message: %s\n", recvbuf);
            printColorText("Received Message: ",recvbuf, FOREGROUND_GREEN);

            // Broadcast the received message to all other clients
            for (int j = 0; j < numClients; j++) {
                if (strcmp(clientList[j].room, client->room) == 0 && clientList[j].socket != ClientSocket) {

                    char broadcastMessage[DEFAULT_BUFLEN + DEFAULT_BUFLEN];

                    // Remove newline character from client->username
                    char* newlinePos = strchr(client->username, '\n');
                    if (newlinePos != NULL) {
                        *newlinePos = '\0';
                    }

                    // Construct the broadcast message with the sender's username and the received message
                    snprintf(broadcastMessage, sizeof(broadcastMessage), "%s: %s", client->username, recvbuf);

                    iSendResult = send(clientList[j].socket, broadcastMessage, strlen(broadcastMessage), 0);
                    if (iSendResult == SOCKET_ERROR) {
                        printf("Send Failed: %d\n", WSAGetLastError());
                    } else {
                        printf("Bytes sent: %d\n", iSendResult);
                    }
                    memset(broadcastMessage, 0, sizeof(broadcastMessage));
                }
            }
        } else if (iResult == 0) {
            printf("Connection closing...\n");
        } else {
            printf("recv Failed (ClientThread): %d\n", WSAGetLastError());
        }
    }  while (iResult > 0);

    EnterCriticalSection(&csClients);

    // Remove the client socket from the list
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
