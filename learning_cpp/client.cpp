#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <limits>
#include <thread>
#include <ios>
#include <string>
#include <sstream>
// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")


#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"

void extractMessage(std::string recvbuf) {
    std::string newline = "\n";
    std::istringstream iss(recvbuf);
    std::string message;
    std::getline(iss, message); // Extract the message part
    std::cout << newline;
    std::cout << message << std::endl;
    return;

}

int receive_Messages(SOCKET ConnectSocket, std::string clientName) {
    char recvbuf[DEFAULT_BUFLEN];
    int iResult;
    int recvbuflen = DEFAULT_BUFLEN;
    while (true) {
        iResult = recv(ConnectSocket, recvbuf, recvbuflen - 1, 0);
        if (iResult > 0) {
            recvbuf[iResult] = '\0'; // Null-terminate the received string
            extractMessage(recvbuf);
            std::cout << clientName << ":";
        }
        else if (iResult == 0) {
            printf("Connection closed by server.\n");
            break;
        }
        else {
            printf("recv failed with error: %d\n", WSAGetLastError());
            closesocket(ConnectSocket);
            WSACleanup();
            return 1;
        }
    }
}

int main(int argc, char** argv)
{
    WSADATA wsaData;
    SOCKET ConnectSocket = INVALID_SOCKET;
    struct addrinfo* result = NULL,
        * ptr = NULL,
        hints;
    char sendbuf[DEFAULT_BUFLEN];
    char recvbuf[DEFAULT_BUFLEN];
    int iResult;
    int recvbuflen = DEFAULT_BUFLEN;

    // Validate the parameters
    if (argc != 2) {
        printf("usage: %s server-name\n", argv[0]);
        return 1;
    }

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve the server address and port
    iResult = getaddrinfo(argv[1], DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    // Attempt to connect to an address until one succeeds
    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

        // Create a SOCKET for connecting to server
        ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
            ptr->ai_protocol);
        if (ConnectSocket == INVALID_SOCKET) {
            printf("socket failed with error: %ld\n", WSAGetLastError());
            WSACleanup();
            return 1;
        }

        // Connect to server.
        iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            closesocket(ConnectSocket);
            ConnectSocket = INVALID_SOCKET;
            continue;
        }
        break;
    }

    freeaddrinfo(result);

    if (ConnectSocket == INVALID_SOCKET) {
        printf("Unable to connect to server!\n");
        WSACleanup();
        return 1;
    }
    bool client_Intro = false;
    //std::thread t1(receive_Messages, ConnectSocket);
    std::thread t1;
    // Loop to continuously send messages to the server
    std::string clientName;
    std::string inputLine;
    while (1) {
        if (client_Intro == false) {
            std::cout << "Welcome to the chatroom! type 'exit' to quit!" << std::endl;
            std::cout << "Enter a username: ";
            std::cin >> clientName;
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            t1 = std::thread(receive_Messages, ConnectSocket, clientName);
            client_Intro = true;
        }
        std::cout << clientName << ": ";
        std::getline(std::cin, inputLine);
        // Check if user wants to exit
        if (inputLine == "exit") {
            printf("Exiting...\n");
            closesocket(ConnectSocket);
            break;
        }
        if (inputLine == "") {
            continue;
        }

        // Send the message to the server
        std::string messageWithClientName = clientName + ":" + inputLine;
        iResult = send(ConnectSocket, messageWithClientName.c_str(), (int)messageWithClientName.length(), 0);
        if (iResult == SOCKET_ERROR) {
            printf("send failed with error: %d\n", WSAGetLastError());
            closesocket(ConnectSocket);
            WSACleanup();
            return 1;
        }
        // Receive response from the server
        /*iResult = recv(ConnectSocket, recvbuf, recvbuflen - 1, 0);
        if (iResult > 0) {
            recvbuf[iResult] = '\0'; // Null-terminate the received string
            std::cout << recvbuf << std::endl;
        }
        else if (iResult == 0) {
            printf("Connection closed by server.\n");
            break;
        }
        else {
            printf("recv failed with error: %d\n", WSAGetLastError());
            closesocket(ConnectSocket);
            WSACleanup();
            return 1;
        }*/
    }
    if (t1.joinable()) {
        t1.join();
    }
    // cleanup
    closesocket(ConnectSocket);
    WSACleanup();

    return 0;
}

