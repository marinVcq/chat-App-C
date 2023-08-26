#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <process.h>
#include "interface.h"

#define DEFAULT_PORT "12345"
#define DEFAULT_BUFLEN 1024

// Controls ID
#define ID_SUBMIT_USERNAME 1001
#define ID_SUBMIT_ROOM 1002
#define ID_CREATE_ROOM 1003
#define ID_VALIDATE_ROOM_CREATION 1004
#define ID_SUBMIT_CHAT 1005

// Declare callback function
unsigned int WINAPI ReceiveThread(LPVOID lpParam);
unsigned int WINAPI SendThread(LPVOID lpParam);

// Global variables
HANDLE recvThread, sendThread;
HWND hTopLabel, hUsernameEdit,hUsernameLabel, hSubmitButton, hErrorContainer,hDescriptionLabel, hKeyFeatures,
hImageControl, hUsernameContainer;

HWND hRoomIdEdit, hPasswordEdit, hSubmitButton, hRoomError, hPasswordInputLabel,
hCreateRoomButton, hRoomInputLabel, hPasswordVerifEdit, hPasswordCheckInputLabel;
HFONT hFont, hHeaderH1Font;
HWND hSubmitButtonChat, hInputText, hChatText;
HBRUSH hBrushErrorLabel;

char sendbuf[DEFAULT_BUFLEN];
char recvbuf[DEFAULT_BUFLEN];
char roomId[DEFAULT_BUFLEN];
char password[DEFAULT_BUFLEN];
char passwordVerif[DEFAULT_BUFLEN];
char username[DEFAULT_BUFLEN];

SOCKET ConnectSocket = INVALID_SOCKET;
struct sockaddr_in serverAddr;
int iResult = 0; // Initialize with an appropriate default value
int hasJoinRoom = 0;
int usernameAvailable = 0;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:

            // Create font
            hHeaderH1Font = CreateFont(30, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
            hFont = CreateFont(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");

            // Create Login Controls
            hTopLabel = CreateWindow("STATIC", welcome, WS_VISIBLE | WS_CHILD | SS_CENTER,50, 20, 450, 30, hwnd, NULL, NULL, NULL);
            hDescriptionLabel = CreateWindow("STATIC",description, WS_VISIBLE | WS_CHILD | SS_CENTER,50, 60, 450, 50, hwnd, NULL, NULL, NULL);
            hUsernameContainer = CreateWindow("STATIC", "", WS_VISIBLE | WS_CHILD | WS_BORDER, 175, 140, 200, 110, hwnd, NULL, NULL, NULL);
            hUsernameLabel = CreateWindow("STATIC","Username: ", WS_VISIBLE | WS_CHILD | SS_CENTER,10, 10, 70, 20, hUsernameContainer, NULL, NULL, NULL);
            hUsernameEdit = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER,10, 40, 180, 30, hUsernameContainer, NULL, NULL, NULL);
            hErrorContainer = CreateWindow("STATIC", "", WS_VISIBLE | WS_CHILD,10, 80, 180, 20, hUsernameContainer, NULL, NULL, NULL);
            
            hSubmitButton = CreateWindow("BUTTON", "Let's Go!", WS_VISIBLE | WS_CHILD,275, 260, 100, 30, hwnd, (HMENU)ID_SUBMIT_USERNAME, NULL, NULL);
            hKeyFeatures = CreateWindow("STATIC",keyFeatures, WS_VISIBLE | WS_CHILD | SS_LEFT,50, 340, 450, 180, hwnd, NULL, NULL, NULL);

            // Set design parameters
            SendMessage(hTopLabel, WM_SETFONT, (WPARAM)hHeaderH1Font, MAKELPARAM(TRUE, 0));
            SendMessage(hDescriptionLabel, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));
            SendMessage(hKeyFeatures, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));
            SendMessage(hErrorContainer, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));
            SendMessage(hErrorContainer, WM_SETTEXT, 0, (LPARAM)""); // Clear initial text
            break;

        case WM_SETCURSOR:
            if ((HWND)wParam == hSubmitButton) {
                SetCursor(LoadCursor(NULL, IDC_HAND));
                return TRUE;
            }
            return DefWindowProc(hwnd, uMsg, wParam, lParam);


        case WM_CTLCOLORSTATIC:
            if ((HWND)lParam == hErrorContainer) {
                // Set text color to red
                SetTextColor((HDC)wParam, RGB(255, 0, 0));
                // Use a transparent background brush for the error label
                hBrushErrorLabel = (HBRUSH)GetStockObject(NULL_BRUSH);
                return (LRESULT)hBrushErrorLabel;
            }
            break;


        case WM_COMMAND:
            if (LOWORD(wParam) == ID_SUBMIT_USERNAME) {
                MessageBox(hwnd, "Submit button clicked!", "Button Click", MB_OK);
                GetWindowText(hUsernameEdit, username, sizeof(username));
                printf("%s",username);

                // Send username to server
                iResult = send(ConnectSocket, username, strlen(username), 0);
                if (iResult == SOCKET_ERROR) {
                    SendMessage(hErrorContainer, WM_SETTEXT, 0, (LPARAM)"Error sending username");
                    printf("Send username failed: %d\n", WSAGetLastError());
                }else{
                    SendMessage(hErrorContainer, WM_SETTEXT, 0, (LPARAM)"");
                }

                // Receive username availability response
                iResult = recv(ConnectSocket, recvbuf, 10, 0);
                if (iResult > 0) {
                    recvbuf[iResult] = '\0';
                    if (strcmp(recvbuf, "available") == 0) {
                        usernameAvailable = 1;
                        printf("Username available. You are now authenticated.\n");
                    } else {
                        SendMessage(hErrorContainer, WM_SETTEXT, 0, (LPARAM)"Username is taken. Please choose another.");
                    }
                } else {
                    SendMessage(hErrorContainer, WM_SETTEXT, 0, (LPARAM)"Receive username availability failed.");
                    printf("Receive username availability failed: %d\n", WSAGetLastError());
                }

                // If username is available, show room ID and password controls
                if (usernameAvailable) {
                    
                    // Destroy previous controls (username input and submit button)
                    DestroyWindow(hUsernameEdit);
                    DestroyWindow(hSubmitButton);
                    DestroyWindow(hErrorContainer);
                    DestroyWindow(hTopLabel);

                    hTopLabel = CreateWindow("STATIC", "Connection to room", WS_VISIBLE | WS_CHILD,
                        50, 20, 200, 20, hwnd, NULL, NULL, NULL);

                    // Create room ID input field
                    hRoomInputLabel = CreateWindow("STATIC", "Room ID: ", WS_VISIBLE | WS_CHILD,
                        50, 50, 200, 20, hwnd, NULL, NULL, NULL);
                    hRoomIdEdit = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER,
                        50, 70, 200, 30, hwnd, NULL, NULL, NULL);

                    // Create password input field
                    hPasswordInputLabel = CreateWindow("STATIC", "Password: ", WS_VISIBLE | WS_CHILD,
                        50, 100, 200, 20, hwnd, NULL, NULL, NULL);
                    hPasswordEdit = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_PASSWORD,
                        50, 120, 200, 30, hwnd, NULL, NULL, NULL);

                    // Create Submit button
                    hSubmitButton = CreateWindow("BUTTON", "Submit", WS_VISIBLE | WS_CHILD,
                        50, 150, 200, 30, hwnd, (HMENU)ID_SUBMIT_ROOM, NULL, NULL);

                    // Create room button
                    hCreateRoomButton = CreateWindow("BUTTON", "Create Room", WS_VISIBLE | WS_CHILD,
                        50, 180, 200, 30, hwnd, (HMENU)ID_CREATE_ROOM, NULL, NULL);
                    
                    // Create error label
                    hRoomError = CreateWindow("STATIC", "", WS_VISIBLE | WS_CHILD,
                        50, 210, 200, 20, hwnd, NULL, NULL, NULL);

                    // Modify UI elements to prompt for room ID and password
                    SetWindowText(hRoomError, ""); // Clear previous error text

                    // Invalidate the window to trigger repainting
                    InvalidateRect(hwnd, NULL, TRUE);
                }
                break;
            }
            else if (LOWORD(wParam) == ID_SUBMIT_ROOM) {
                char roomId[DEFAULT_BUFLEN];
                char password[DEFAULT_BUFLEN];
                GetWindowText(hRoomIdEdit, roomId, sizeof(roomId));
                GetWindowText(hPasswordEdit, password, sizeof(password));

                // Send room ID to the server
                iResult = send(ConnectSocket, roomId, strlen(roomId), 0);
                if (iResult == SOCKET_ERROR) {
                    SendMessage(hRoomError, WM_SETTEXT, 0, (LPARAM)"Send room ID failed.");
                }

                // Receive room availability response send the password
                iResult = recv(ConnectSocket, recvbuf, 10, 0);
                if (iResult > 0) {
                    recvbuf[iResult] = '\0';

                    if (strcmp(recvbuf, "0") == 0) {
                        SendMessage(hRoomError, WM_SETTEXT, 0, (LPARAM)"Provide an existing Room ID.");
                    }
                    else if (strcmp(recvbuf, "1") == 0) {
                        printf("Room exist\n");
                        // Send Password to the server
                        iResult = send(ConnectSocket, password, strlen(password), 0);
                        if (iResult == SOCKET_ERROR) {
                            SendMessage(hRoomError, WM_SETTEXT, 0, (LPARAM)"Send Password failed.");
                        }                
                        
                        // Get response
                        iResult = recv(ConnectSocket, recvbuf, DEFAULT_BUFLEN, 0);
                        if (iResult > 0) {
                            recvbuf[iResult] = '\0';
                            if (strcmp(recvbuf, "1") == 0) {
                                printf("connected to room\n");
                                // Set dialogue Box
                                // Destroy previous controls (username input and submit button)
                                DestroyWindow(hRoomIdEdit);
                                DestroyWindow(hPasswordEdit);
                                DestroyWindow(hCreateRoomButton);
                                DestroyWindow(hPasswordVerifEdit);
                                DestroyWindow(hPasswordCheckInputLabel);
                                DestroyWindow(hRoomInputLabel);
                                DestroyWindow(hPasswordInputLabel);
                                DestroyWindow(hTopLabel);

                                // Create chat text zone, input text field, and submit button
                                hTopLabel = CreateWindow("STATIC", "connected", WS_VISIBLE | WS_CHILD,
                                    25, 20, 200, 20, hwnd, NULL, NULL, NULL);
                                hChatText = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | WS_VSCROLL | ES_MULTILINE,
                                    25, 50, 320, 200, hwnd, NULL, NULL, NULL);
                                hInputText = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER,
                                    25, 260, 250, 20, hwnd, NULL, NULL, NULL);
                                hSubmitButtonChat = CreateWindow("BUTTON", "Send", WS_VISIBLE | WS_CHILD,
                                    300, 260, 50, 20, hwnd, (HMENU)ID_SUBMIT_CHAT, NULL, NULL);
                                MoveWindow(hRoomError, 25, 310, 250, 30, TRUE);

                                // Start the receiving thread for chat messages
                                recvThread = (HANDLE)_beginthreadex(NULL, 0, ReceiveThread, &ConnectSocket, 0, NULL);
                                if (recvThread == NULL) {
                                    // Handle thread creation error
                                    // ...
                                }
                            } else {
                                SendMessage(hRoomError, WM_SETTEXT, 0, (LPARAM)"Fail to connect.");
                            }
                        }
                    }else {
                        SendMessage(hRoomError, WM_SETTEXT, 0, (LPARAM)"Access denied. something went wrong.");
                    } 
                }else {
                    SendMessage(hRoomError, WM_SETTEXT, 0, (LPARAM)"Receive room authorization failed");
                }
            }
            // Create Room 
            else if (LOWORD(wParam) == ID_CREATE_ROOM) {

                char temp[] = "create room";
                // Inform server about room creation process
                iResult = send(ConnectSocket, temp, strlen(temp), 0);
                if (iResult == SOCKET_ERROR) {
                    SendMessage(hRoomError, WM_SETTEXT, 0, (LPARAM)"Send create room request failed.");
                }

                // Destroy previous controls (username input and submit button)
                DestroyWindow(hSubmitButton);
                DestroyWindow(hCreateRoomButton);
                DestroyWindow(hTopLabel);

                // Create control 
                hTopLabel = CreateWindow("STATIC", "Create room:", WS_VISIBLE | WS_CHILD,
                        50, 20, 200, 20, hwnd, NULL, NULL, NULL);

                // Create password input field
                hPasswordCheckInputLabel = CreateWindow("STATIC", "Check Password: ", WS_VISIBLE | WS_CHILD,
                    50, 150, 200, 20, hwnd, NULL, NULL, NULL);
                hPasswordVerifEdit = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_PASSWORD,
                    50, 170, 200, 30, hwnd, NULL, NULL, NULL);

                // Create room button
                hCreateRoomButton = CreateWindow("BUTTON", "Validate", WS_VISIBLE | WS_CHILD,
                    50, 210, 200, 30, hwnd, (HMENU)ID_VALIDATE_ROOM_CREATION, NULL, NULL);

                MoveWindow(hRoomError, 50, 250, 200, 30, TRUE);
            } 
            
            // Room creation
            else if(LOWORD(wParam) == ID_VALIDATE_ROOM_CREATION){
                GetWindowText(hRoomIdEdit, roomId, sizeof(roomId));
                GetWindowText(hPasswordEdit, password, sizeof(password));
                GetWindowText(hPasswordVerifEdit, passwordVerif, sizeof(passwordVerif));

                // Send room ID to server
                iResult = send(ConnectSocket, roomId, strlen(roomId), 0);
                if (iResult == SOCKET_ERROR) {
                    SendMessage(hRoomError, WM_SETTEXT, 0, (LPARAM)"Send room ID failed.");
                }
                
                // Send Password to the server
                if (strcmp(password, passwordVerif) == 0){
                    iResult = send(ConnectSocket, password, strlen(password), 0);
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
                            
                            // Set dialogue Box
                            // Destroy previous controls (username input and submit button)
                            DestroyWindow(hRoomIdEdit);
                            DestroyWindow(hPasswordEdit);
                            DestroyWindow(hCreateRoomButton);
                            DestroyWindow(hSubmitButton);
                            DestroyWindow(hRoomInputLabel);
                            DestroyWindow(hPasswordInputLabel);
                            DestroyWindow(hTopLabel);

                            // Create chat text zone, input text field, and submit button
                            hTopLabel = CreateWindow("STATIC", "connected", WS_VISIBLE | WS_CHILD,
                                25, 20, 200, 20, hwnd, NULL, NULL, NULL);
                            hChatText = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | WS_VSCROLL | ES_MULTILINE,
                                25, 50, 320, 200, hwnd, NULL, NULL, NULL);
                            hInputText = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER,
                                25, 260, 250, 20, hwnd, NULL, NULL, NULL);
                            hSubmitButtonChat = CreateWindow("BUTTON", "Send", WS_VISIBLE | WS_CHILD,
                                300, 260, 50, 20, hwnd, (HMENU)ID_SUBMIT_CHAT, NULL, NULL);
                            MoveWindow(hRoomError, 25, 310, 250, 30, TRUE);

                            // Start the receiving thread for chat messages
                            recvThread = (HANDLE)_beginthreadex(NULL, 0, ReceiveThread, &ConnectSocket, 0, NULL);
                            if (recvThread == NULL) {
                                // Handle thread creation error
                                // ...
                            }

                        } else {
                            printf("Something fail in the room creation process.\n");
                            break;
                        }
                    }
                }                    
            }
            else if(LOWORD(wParam) == ID_SUBMIT_CHAT){
                char chatMessage[DEFAULT_BUFLEN];
                GetWindowText(hInputText, chatMessage, sizeof(chatMessage));

                // Send the chat message to the server
                iResult = send(ConnectSocket, chatMessage, strlen(chatMessage), 0);
                if (iResult == SOCKET_ERROR) {
                    // Handle error sending chat message
                    // ...
                }else {
                    // Append the user's sent message to the chatText
                    SendMessage(hChatText, EM_SETSEL, -1, -1); // Move caret to end
                    SendMessage(hChatText, EM_REPLACESEL, FALSE, (LPARAM)"You: ");
                    SendMessage(hChatText, EM_REPLACESEL, FALSE, (LPARAM)chatMessage);
                    SendMessage(hChatText, EM_REPLACESEL, FALSE, (LPARAM)"\r\n"); // Add newline
                }

                // Clear the input text field
                SetWindowText(hInputText, "");
            }

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
        CW_USEDEFAULT, CW_USEDEFAULT,550,600, NULL, NULL, hInstance, NULL);
    
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

            printf("receive message: %s\n", recvbuf);

            // Get server response
            int textLength = GetWindowTextLength(hChatText);

            // Append received message to existing text
            SendMessage(hChatText, EM_SETSEL, textLength, textLength);
            SendMessage(hChatText, EM_REPLACESEL, FALSE, (LPARAM)recvbuf);

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
