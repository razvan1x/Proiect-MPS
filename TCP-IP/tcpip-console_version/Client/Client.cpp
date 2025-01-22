#include <iostream>
#include <string>
#include <WS2tcpip.h>
#include <WinSock2.h>

#pragma comment(lib, "Ws2_32.lib")

const int BUFFER_SIZE = 1024;

void run_client(const std::string& server_ip, int server_port) {
    WSADATA wsData;
    if (WSAStartup(MAKEWORD(2, 2), &wsData) != 0) {
        std::cerr << "Error in initializing Winsock" << std::endl;
        return;
    }

    SOCKET client_socket;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    int bytes_received;

    // Create socket
    client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client_socket == INVALID_SOCKET) {
        std::cerr << "Error in creating socket" << std::endl;
        WSACleanup();
        return;
    }

    // Specify server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr);

    // Connect to server
    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        std::cerr << "Error in connecting to server" << std::endl;
        closesocket(client_socket);
        WSACleanup();
        return;
    }

    std::cout << "Connected to server" << std::endl;

    // Get client ID
    int client_id;
    std::cout << "Enter your client ID: ";
    std::cin >> client_id;

    // Send client ID to server
    send(client_socket, reinterpret_cast<char*>(&client_id), sizeof(client_id), 0);

    // Get IP address and port of server
    char server_ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(server_addr.sin_addr), server_ip_str, INET_ADDRSTRLEN);
    int server_port_num = ntohs(server_addr.sin_port);
    std::cout << "Server IP: " << server_ip_str << ", Port: " << server_port_num << std::endl;

    // Ask user for target client ID
    int target_client_id;
    std::cout << "Enter the ID of the client you want to communicate with: ";
    std::cin >> target_client_id;

    // Send target client ID to server
    send(client_socket, reinterpret_cast<char*>(&target_client_id), sizeof(target_client_id), 0);

    // Communicate with server
    while (true) {
        std::cout << "Enter message: ";
        std::string input;
        std::getline(std::cin, input);

        // Truncate the message if it exceeds the buffer size
        if (input.length() >= BUFFER_SIZE) {
            std::cerr << "Message too long. Truncating..." << std::endl;
            input = input.substr(0, BUFFER_SIZE - 1);
        }

        // Send data to server along with target client ID
        send(client_socket, reinterpret_cast<char*>(&target_client_id), sizeof(target_client_id), 0);
        send(client_socket, input.c_str(), input.length(), 0);

        // Receive response from server
        bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received < 0) {
            std::cerr << "Error in receiving data from server" << std::endl;
            break;
        }
        else if (bytes_received == 0) {
            std::cout << "Server disconnected" << std::endl;
            break;
        }
        buffer[bytes_received] = '\0';
        std::cout << "Received from server: " << buffer << std::endl;
    }

    closesocket(client_socket);
    WSACleanup();
}

int main() {
    std::string server_ip;
    int server_port;

    std::cout << "Enter server IP address: ";
    std::cin >> server_ip;
    std::cout << "Enter server port: ";
    std::cin >> server_port;

    run_client(server_ip, server_port);

    return 0;
}
