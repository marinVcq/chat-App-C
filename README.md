# Winsock application
There are two types of socket network applications: Server and Client

## Abstract and weird things

The Winsock2.h header file internally includes core elements from the Windows.h header file, so there is not usually an #include line for the Windows.h header file in Winsock applications. If an #include line is needed for the Windows.h header file, this should be preceded with the #define WIN32_LEAN_AND_MEAN macro. For historical reasons, the Windows.h header defaults to including the Winsock.h header file for Windows Sockets 1.1. The declarations in the Winsock.h header file will conflict with the declarations in the Winsock2.h header file required by Windows Sockets 2.0. The WIN32_LEAN_AND_MEAN macro prevents the Winsock.h from being included by the Windows.h header.

## Server 

### Server Workflow

1. Initialize Winsock
2. Create a Socket
3. Bind a Socket
4. Listen for multiple clients
5. Accept a connection from a client
6. Receive and send data 
7. Disconect

## Client

### Client Workflow

1. Initialize winsock
2. Create a socket
3. Connect to a server
4. Send and receive data
5. Disconect

## Common part between Server and Client

### Initialization of Winsock 

1. Create a WSADATA object

this object is useful to set up global parameters like the maximum number of sockets openened or the version of winsock.

2. Use WSAStartup function

The WSAStartup function is called to initiate use of WS2_32.dll. This function initiates the use of the Windows Sockets DLL by a process. The WSAStartup function returns a pointer to the
WSADATA structure in the lpWSAData parameter.

More Details: https://learn.microsoft.com/en-us/windows/win32/api/winsock/ns-winsock-wsadata

### Create Socket

After initialization, a SOCKET object must be instantiated for use by the server, I put the adress resolution and hosting set up in this section but its still part of initialization.

1. Adress Resolution

Resolve the local address and port to be used by the server with getaddrinfo() More details about this function: https://learn.microsoft.com/en-us/windows/win32/api/ws2tcpip/nf-ws2tcpip-getaddrinfo

getaddrinfo() est utilisée pour résoudre un nom d'hôte (tel qu'un nom de domaine) et un service (tel qu'un port) en une liste de structures d'adresses réseau compatibles avec l'hôte et le service spécifiés. Cette fonction est utile lorsque vous devez établir une connexion réseau avec un autre hôte en spécifiant son nom d'hôte et son port.

First we have to set up the host information, for that purpose we use the addrinfo structure. More details about addrinfo here: https://learn.microsoft.com/en-us/windows/win32/api/ws2def/ns-ws2def-addrinfoa

2. Create the Socket

Simply create a SOCKET object called listenSocket or clientSocket or whatever you want.

We set up host information (protocol, adress, port...) with getaddrinfo and fill the result structure. the socket should be based on this structure.

**Error handling:**

Specific function can be used to figure out error
The WSAGetLastError function returns the error status for the last Windows Sockets operation that failed. More details here: https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsagetlasterror

**Optional things**
You can set some socket option before binding with setsockopt function. More details here: https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-setsockopt

3. Bind Socket 

For a server to accept client connections, it must be bound to a network address within the system. For server purpose its means associates a local address with a socket. More details here: https://learn.microsoft.com/en-us/windows/win32/api/winsock/nf-winsock-bind

4. Listen Socket

places the server socket in a state in which it is listening for an incoming client connection.

5. Accepting a connection

Normally a server application would be designed to listen for connections from multiple clients. For high-performance servers, multiple threads are commonly used to handle the multiple client connections.

Les fonctions accept(), AcceptEx() et WSAAccept() sont spécifiques à différentes interfaces et systèmes. Voici une explication de chacune de ces fonctions :

**accept():** Cette fonction fait partie des appels systèmes standard de la programmation de sockets en C/C++. Elle est utilisée dans les systèmes basés sur les sockets BSD (Berkeley Software Distribution), tels que les systèmes Unix et Unix-like (Linux, macOS, etc.). La fonction accept() est utilisée pour accepter une nouvelle connexion entrante sur un socket en attente et créer un nouveau socket qui représente cette connexion. Elle retourne un nouveau descripteur de socket que vous pouvez utiliser pour communiquer avec le client connecté. Cette fonction est relativement simple et est bien prise en charge sur les systèmes de type Unix.

AcceptEx(): Cette fonction est spécifique à la plateforme Windows et fait partie de l'API Winsock (Windows Sockets). AcceptEx() est conçue pour des performances accrues et une utilisation plus efficace des E/S asynchrones. Elle permet de pré-accepter une connexion, c'est-à-dire de préallouer des ressources pour une connexion entrante et de traiter la création du socket une fois que les données sont prêtes. Cela peut être utile dans des scénarios où vous gérez de nombreuses connexions simultanées, car cela peut réduire les opérations coûteuses liées à la création et à la suppression des sockets. AcceptEx() est principalement utilisée dans les applications Windows et est bien adaptée à la programmation asynchrone.

WSAAccept(): Cette fonction fait également partie de l'API Winsock (Windows Sockets) et est similaire à accept(), mais elle est conçue pour être utilisée avec des sockets non-bloquants et des opérations E/S asynchrones. WSAAccept() offre un moyen d'accepter une connexion sur un socket non-bloquant et de créer un nouveau socket associé à cette connexion. Elle permet de gérer des connexions entrantes tout en maintenant un modèle de programmation asynchrone. Comme AcceptEx(), WSAAccept() est principalement utilisée dans des applications Windows.

En résumé, la principale différence entre ces fonctions réside dans leur compatibilité avec les systèmes d'exploitation (Unix vs Windows), leurs performances et leur adaptation à des modèles de programmation spécifiques tels que l'asynchrone.

Next Step: Advanced Socket programming: https://learn.microsoft.com/en-us/windows/win32/winsock/getting-started-with-winsock



I want to maintain the ability to send multiple messages in a row, even while waiting for messages to be received in the ReceiveThread. To achieve this, we can use a separate thread for sending messages. Here's how you can structure your code:









