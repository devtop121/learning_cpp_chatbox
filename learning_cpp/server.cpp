#undef UNICODE
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <thread>
#include <vector>
#include <iostream> 
#include <mutex>
#include <algorithm>
#include <string>

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"
#define TIMEOUT_MS 10000

struct ClientInfo {
    SOCKET socket;
    std::string name;
    // Add other client-related information here
};

// Removes a client from the users vector
void removeClient(SOCKET ClientSocket, std::vector<ClientInfo>& users, std::mutex& users_mutex) {
    std::lock_guard<std::mutex> lock(users_mutex);
    users.erase(std::remove_if(users.begin(), users.end(),
        [ClientSocket](const ClientInfo& user) { return user.socket == ClientSocket; }), users.end());
}

void clientThread(SOCKET ClientSocket, std::vector<ClientInfo>& users, std::mutex& users_mutex) {
    char recvbuf[DEFAULT_BUFLEN];
    int recvbuflen = DEFAULT_BUFLEN;

    // Receive the initial message containing the client name
    int iResult = recv(ClientSocket, recvbuf, recvbuflen - 1, 0);
    if (iResult <= 0) {
        printf("Failed to receive client name.\n");
        closesocket(ClientSocket);
        return;
    }
    recvbuf[iResult] = '\0';

    // Extract client name from the received message
    std::string receivedData(recvbuf);
    size_t colonPos = receivedData.find(':');
    if (colonPos == std::string::npos || colonPos == 0 || colonPos >= receivedData.length() - 1) {
        printf("Invalid message format.\n");
        closesocket(ClientSocket);
        return;
    }

    std::string clientName = receivedData.substr(0, colonPos);
    

    // Notify that a client has connected
    std::string newline = "\n";
    std::cout << newline;
    printf("Client %s connected.\n", clientName.c_str());

    // Add the client to the users vector
    {
        std::lock_guard<std::mutex> guard(users_mutex);
        users.push_back({ ClientSocket, clientName });
    }
    int bytesReceived = 0;
    // Main loop to handle communication with the client
    while (true) {
        bytesReceived = 0;
        // Receive message from the client
        iResult = recv(ClientSocket, recvbuf, recvbuflen - 1, 0);
        if (iResult > 0) {
            bytesReceived += iResult;
            if (bytesReceived >= recvbuflen - 1) {
                std::cout << "Buffer overflow detected.";
                break;
            }
            recvbuf[iResult] = '\0';

            std::string newline = "\n";
            std::cout << newline;
            recvbuf[iResult] = '\0';
            std::cout << "Received message from: " << clientName.c_str();
            std::cout << newline;
            for (int i = 0; i < iResult; ++i) {
                if (recvbuf[i] == '\n') {
                    std::cout << std::endl;
                    break;
                }
                std::cout << recvbuf[i];
            }

            // Forward the message to other clients
            {
                std::lock_guard<std::mutex> lock(users_mutex);
                for (const auto& user : users) {
                    if (user.socket != ClientSocket) {
                        int sendResult = send(user.socket, recvbuf, iResult, 0);
                        if (sendResult == SOCKET_ERROR) {
                            printf("send failed with error: %d\n", WSAGetLastError());
                            break;
                        }
                    }
                }
            }
        }
        else {
            if (iResult == 0) {
                printf("Connection closed by peer.\n");
            }
            else {
                printf("recv failed with error: %d\n", WSAGetLastError());
            }
            // Remove the client from the users vector
            removeClient(ClientSocket, users, users_mutex);
            closesocket(ClientSocket);
            printf("Removed user %s from list.\n", clientName.c_str());
            return;
        }
    }
}

int main() {
    WSADATA wsaData;
    int iResult;

    SOCKET ListenSocket = INVALID_SOCKET;
    SOCKET ClientSocket = INVALID_SOCKET;

    struct addrinfo* result = NULL;
    struct addrinfo hints;

    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }
    // Create a SOCKET for the server to listen for client connections.
    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    // Setup the TCP listening socket
    iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }
    freeaddrinfo(result);
    iResult = listen(ListenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        printf("listen failed with error: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }
    
    std::vector<ClientInfo> users;
    std::vector<std::thread> threads;
    std::mutex users_mutex;

    while (true) {
        // Accept a client connection
        ClientSocket = accept(ListenSocket, NULL, NULL);
        if (ClientSocket == INVALID_SOCKET) {
            printf("accept failed with error: %d\n", WSAGetLastError());
            closesocket(ListenSocket);
            WSACleanup();
            return 1;
        }
        // Create a thread to handle communication with this client
        threads.emplace_back([&users, &users_mutex](SOCKET socket) {
            // Now we pass the mutex to the thread function as well
            clientThread(socket, users, users_mutex);
            }, ClientSocket);
    }

    // Close the listening socket when done
    closesocket(ListenSocket);
    WSACleanup();

    return 0;
}
