#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <process.h>

#define DEFAULT_PORT "12345"
#define DEFAULT_BUFLEN 1024
#define ID_SUBMIT 1001

// Declare callback function
unsigned int WINAPI ReceiveThread(LPVOID lpParam);
unsigned int WINAPI SendThread(LPVOID lpParam);

// Global variables
HANDLE recvThread;
HANDLE sendThread;
HWND hUsernameEdit, hSubmitButton;

char sendbuf[DEFAULT_BUFLEN];
char recvbuf[DEFAULT_BUFLEN];
SOCKET ConnectSocket = INVALID_SOCKET;
struct sockaddr_in serverAddr;
int iResult = 0; // Initialize with an appropriate default value

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            // Create label for message
            CreateWindow("STATIC", "Please login to continue", WS_VISIBLE | WS_CHILD,
                50, 20, 200, 20, hwnd, NULL, NULL, NULL);
            
            // Create username edit box
            hUsernameEdit = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER,
                50, 50, 200, 30, hwnd, NULL, NULL, NULL);
            
            // Create submit button with unique ID
            hSubmitButton = CreateWindow("BUTTON", "Submit", WS_VISIBLE | WS_CHILD,
                50, 90, 200, 30, hwnd, (HMENU)ID_SUBMIT, NULL, NULL);
            break;


        case WM_COMMAND:
            if (LOWORD(wParam) == ID_SUBMIT) {
                char username[DEFAULT_BUFLEN];
                int usernameAvailable = 0;
                GetWindowText(hUsernameEdit, username, sizeof(username));

                // Send username to server
                iResult = send(ConnectSocket, username, strlen(username), 0);
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
                        usernameAvailable = 1;
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

                // Handle username availability response
                // ...

                // If username is available, show room ID and password controls
                if (usernameAvailable) {
                    // Create controls (hRoomIdEdit, hPasswordEdit, hSubmitButton)
                    // ...

                    // Modify UI elements to prompt for room ID and password
                    // ...

                }

                break;
            }
            // Handle other button clicks
            // ...

            break;

        case WM_CLOSE:
            DestroyWindow(hwnd);
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;
    }
    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        MessageBox(NULL, "WSAStartup failed", "Error", MB_ICONERROR);
        return 1;
    }else{
        MessageBox(NULL, "WSAStartup okay", "info",  MB_OK);
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

    // Create main window
    HWND hWnd;
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = (HBRUSH)(COLOR_BACKGROUND);
    wc.lpszClassName = "MyWindowClass";
    
    if (!RegisterClass(&wc)) {
        MessageBox(NULL, "Window registration failed", "Error", MB_ICONERROR);
        return 1;
    }
    
    hWnd = CreateWindowEx(0, "MyWindowClass", "My App", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, NULL, NULL, hInstance, NULL);
    
    if (!hWnd) {
        MessageBox(NULL, "Window creation failed", "Error", MB_ICONERROR);
        return 1;
    }

    // Show and update the main window
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Cleanup and close Winsock
    WSACleanup();

    return (int)msg.wParam;
}


// Threads for receiving and sending messages
unsigned int WINAPI ReceiveThread(LPVOID lpParam) {
    SOCKET ConnectSocket = *((SOCKET*)lpParam);
    char recvbuf[DEFAULT_BUFLEN];
    int iResult;

    while (1) {
        iResult = recv(ConnectSocket, recvbuf, DEFAULT_BUFLEN, 0);
        if (iResult > 0) {
            recvbuf[iResult] = '\0';

            // Get server response

            // Recreate the SendThread
            HANDLE newSendThread = (HANDLE)_beginthreadex(NULL, 0, SendThread, &ConnectSocket, 0, NULL);
            if (newSendThread != NULL) {
                // Close the old sendThread handle
                CloseHandle(sendThread);
                sendThread = newSendThread;
            }
            memset(recvbuf, 0, sizeof(recvbuf));
        } else if (iResult == 0) {
            // display error: "Connection closed by server"
            break;
        } else {
            // Display error; "Receive failed"
            break;
        }
    }
    return 0;
}

unsigned int WINAPI SendThread(LPVOID lpParam) {
    SOCKET ConnectSocket = *((SOCKET*)lpParam);
    char sendbuf[DEFAULT_BUFLEN];

    while (1) {
        fgets(sendbuf, DEFAULT_BUFLEN, stdin);
        fflush(stdout);

        // Send data to the server if message is not empty
        if (strlen(sendbuf) > 0) {
            int iResult = send(ConnectSocket, sendbuf, strlen(sendbuf), 0);
            if (iResult == SOCKET_ERROR) {
                // Display error: Send failed"
                break;
            }
        } else {
            // Display error: "Message cannot be empty"
        }

        memset(sendbuf, 0, sizeof(sendbuf));
    }

    return 0;
}
