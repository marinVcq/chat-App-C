#include "interface.h"
#include <stdio.h>

// Text Content
const char *description = "Connect, Chat, Collaborate! Developed using \
C programming and powered by the Winsock2 networking library and Windows.h user interface framework.";
const char *welcome = "Welcome to the C-Based Chat App";
const char *keyFeatures =
    "Key Features:\r\n\r\n"
    "Streamlined Conversations: Connect with other users through real-time messaging in a WIN32 GUI environment.\r\n"
    "Room Collaboration: Create or join rooms, enabling you to focus discussions on specific groups.\r\n"
    "Unified Broadcast: Every message is instantly broadcasted through a socket server to all participants in that room.\r\n"
    "Dependable Performance: Built on the foundation of C programming, expect an efficient chat application.\r\n";
const char *roomInfo = 
    "Server Broadcast and Room Workflow:\r\n\r\n"
    "Room Selection:\r\n"
    "If you choose to join an existing room, the server verifies the room's existence and room's password for authentication.\r\n"
    "Room Creation:\r\n"
    "If you choose to create a new room, the server receives the room ID and the associate password. The server checks if the maximum number of rooms is not reach and then creates a new room.\r\n"
    "Room Management:\r\n"
    "Each room maintains a list of connected clients. The server iterates through the list of connected clients in that room and broadcasts the message to all other clients.\r\n";
