#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <process.h>

#define DEFAULT_PORT "12345"
#define DEFAULT_BUFLEN 1024

unsigned int WINAPI ReceiveThread(LPVOID lpParam);
unsigned int WINAPI SendThread(LPVOID lpParam);

HANDLE recvThread;
HANDLE sendThread;
HANDLE consoleMutex;

int printColorText(const char *text, const char *buffer, int colorCode){
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, colorCode);
    printf("%s %s",text, buffer);
    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    return 0;
}

int main() {

    WSADATA wsaData;
    SOCKET ConnectSocket = INVALID_SOCKET;
    struct sockaddr_in serverAddr;
    int iResult;
    consoleMutex = CreateMutex(NULL, FALSE, NULL);
    setbuf(stdout, NULL);

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
    int hasJoinRoom = 0;

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

    while(!hasJoinRoom){
        printf("Enter room ID: ");
        fgets(sendbuf, DEFAULT_BUFLEN, stdin);

        // Send room ID to the server
        iResult = send(ConnectSocket, sendbuf, strlen(sendbuf), 0);
        if (iResult == SOCKET_ERROR) {
            printf("Send room ID failed: %d\n", WSAGetLastError());
            break;
        }

        // Receive room availability response send the password
        iResult = recv(ConnectSocket, recvbuf, 10, 0);
        if (iResult > 0) {
            recvbuf[iResult] = '\0';

            char password[DEFAULT_BUFLEN];
            char passwordVerif[DEFAULT_BUFLEN];

            // Case 0: Room doesn't exists 
            if (strcmp(recvbuf, "0") == 0) {

                printf("Create room: \n");
                printf("Password: ");
                fgets(password, DEFAULT_BUFLEN, stdin);
                printf("Confirm password: ");
                fgets(passwordVerif, DEFAULT_BUFLEN, stdin);


                if (strcmp(password, passwordVerif) == 0){
                    // Send Password to the server
                    iResult = send(ConnectSocket, sendbuf, strlen(sendbuf), 0);
                    if (iResult == SOCKET_ERROR) {
                        printf("Send password failed: %d\n", WSAGetLastError());
                        break;
                    }
                    // Get response
                    iResult = recv(ConnectSocket, recvbuf, DEFAULT_BUFLEN, 0);
                    if (iResult > 0) {
                        recvbuf[iResult] = '\0';
                        if (strcmp(recvbuf, "1") == 0) {
                            printf("Connected to room.\n");
                            hasJoinRoom = 1;
                        } else {
                            printf("Something fail in the room creation process.\n");
                            break;
                        }
                    }
                }
            
            }
            // Case 1: Room already exists 
            else if (strcmp(recvbuf, "1") == 0) { // Case 1 the room exist

                printf("Password: ");
                fgets(sendbuf, DEFAULT_BUFLEN, stdin);

                // Send Password to the server
                iResult = send(ConnectSocket, sendbuf, strlen(sendbuf), 0);
                if (iResult == SOCKET_ERROR) {
                    printf("Send password failed: %d\n", WSAGetLastError());
                    break;
                }

                // Get response
                iResult = recv(ConnectSocket, recvbuf, DEFAULT_BUFLEN, 0);
                if (iResult > 0) {
                    recvbuf[iResult] = '\0';
                    if (strcmp(recvbuf, "1") == 0) {
                        printf("Connected to room.\n");
                        hasJoinRoom = 1;
                    } else {
                        printf("Fail to connect.\n");
                }
            }else {
                printf("Access denied. something went wrong.\n");
                break;
            }  
        } else {
            printf("Receive room authorization failed: %d\n", WSAGetLastError());
            break;
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
    CloseHandle(recvThread);
    CloseHandle(sendThread);
    CloseHandle(consoleMutex);
    WSACleanup();
    return 0;
    }
}


unsigned int WINAPI ReceiveThread(LPVOID lpParam) {
    SOCKET ConnectSocket = *((SOCKET*)lpParam);
    char recvbuf[DEFAULT_BUFLEN];
    int iResult;

    while (1) {
        iResult = recv(ConnectSocket, recvbuf, DEFAULT_BUFLEN, 0);
        if (iResult > 0) {
            recvbuf[iResult] = '\0';

            // Lock the mutex to prevent conflicts with console output
            WaitForSingleObject(consoleMutex, INFINITE);
            printColorText("\n",recvbuf, FOREGROUND_BLUE);
            printf("\n");
            fflush(stdout);
            // Release the mutex to allow other threads to access the console
            ReleaseMutex(consoleMutex);

            // Recreate the SendThread
            HANDLE newSendThread = (HANDLE)_beginthreadex(NULL, 0, SendThread, &ConnectSocket, 0, NULL);
            if (newSendThread != NULL) {
                // Close the old sendThread handle
                CloseHandle(sendThread);
                sendThread = newSendThread;
            }
            memset(recvbuf, 0, sizeof(recvbuf));
        } else if (iResult == 0) {
            printf("Connection closed by server.\n");
            break;
        } else {
            printf("Receive failed: %d\n", WSAGetLastError());
            break;
        }
    }

    return 0;
}

unsigned int WINAPI SendThread(LPVOID lpParam) {
    SOCKET ConnectSocket = *((SOCKET*)lpParam);
    char sendbuf[DEFAULT_BUFLEN];

    while (1) {
        WaitForSingleObject(consoleMutex, INFINITE);
        printColorText("You: ", "", FOREGROUND_GREEN);
        ReleaseMutex(consoleMutex);

        fgets(sendbuf, DEFAULT_BUFLEN, stdin);
        fflush(stdout);

        // Trim newline character
        sendbuf[strcspn(sendbuf, "\n")] = '\0';

        // Send data to the server if message is not empty
        if (strlen(sendbuf) > 0) {
            int iResult = send(ConnectSocket, sendbuf, strlen(sendbuf), 0);
            if (iResult == SOCKET_ERROR) {
                printf("Send failed: %d\n", WSAGetLastError());
                break;
            }
        } else {
            WaitForSingleObject(consoleMutex, INFINITE);
            printf("Message cannot be empty.\n");
            ReleaseMutex(consoleMutex);
        }

        memset(sendbuf, 0, sizeof(sendbuf));
    }

    return 0;
}


