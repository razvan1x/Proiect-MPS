#include <iostream>
#include <string>
#include <WS2tcpip.h>
#include <WinSock2.h>
#include <thread>
#include <map>
#include <mutex>

#pragma comment(lib, "Ws2_32.lib")

const int BUFFER_SIZE = 1024;

std::map<int, SOCKET> clients;
std::mutex clients_mutex;

void client_handler(SOCKET client_socket) {
    char buffer[BUFFER_SIZE];
    int bytes_received;

    // Receive client ID from client
    int client_id;
    bytes_received = recv(client_socket, reinterpret_cast<char*>(&client_id), sizeof(client_id), 0);
    if (bytes_received <= 0) {
        std::cerr << "Error in receiving client ID from client" << std::endl;
        closesocket(client_socket);
        return;
    }

    // Notify server about client's connection
    sockaddr_in client_info;
    int len = sizeof(client_info);
    if (getpeername(client_socket, (sockaddr*)&client_info, &len) == 0) {
        char client_ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_info.sin_addr), client_ip_str, INET_ADDRSTRLEN);
        int client_port_num = ntohs(client_info.sin_port);
        std::cout << "Client " << client_id << " connected from IP: " << client_ip_str << ", Port: " << client_port_num << std::endl;
    }

    // Adaugă clientul în map-ul clients împreună cu ID-ul său
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        clients[client_id] = client_socket;
    }

    while (true) {
        // Receive target client ID from client
        int target_client_id;
        bytes_received = recv(client_socket, reinterpret_cast<char*>(&target_client_id), sizeof(target_client_id), 0);
        if (bytes_received <= 0) {
            std::cerr << "Error in receiving target client ID from client " << client_id << std::endl;
            break;
        }

        // Receive message from client
        bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            std::cerr << "Error in receiving data from client " << client_id << std::endl;
            break;
        }

        // Find target client
        SOCKET target_client_socket;
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            auto it = clients.find(target_client_id);
            if (it != clients.end()) {
                target_client_socket = it->second;
                std::cout << "Client " << target_client_id << " found" << std::endl;
            }
            else {
                std::cerr << "Client " << target_client_id << " not found" << std::endl;
                continue;
            }
        }

        // Forward message to target client

        // Add sender's ID to the message
        std::string message_with_sender = "From client " + std::to_string(client_id) + ": " + std::string(buffer, bytes_received);

        if (send(target_client_socket, message_with_sender.c_str(), message_with_sender.length(), 0) == SOCKET_ERROR) {
            std::cerr << "Error in sending data to client " << target_client_id << std::endl;
        }
    }

    // Remove client from the map
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        clients.erase(client_id);
    }
    closesocket(client_socket);
}

void run_server(int port) {
    WSADATA wsData;
    if (WSAStartup(MAKEWORD(2, 2), &wsData) != 0) {
        std::cerr << "Error in initializing Winsock" << std::endl;
        return;
    }

    SOCKET server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    int client_len = sizeof(client_addr);

    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == INVALID_SOCKET) {
        std::cerr << "Error in creating socket" << std::endl;
        WSACleanup();
        return;
    }

    // Specify server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // Bind socket
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        std::cerr << "Error in binding socket" << std::endl;
        closesocket(server_socket);
        WSACleanup();
        return;
    }

    // Listen for incoming connections
    if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Error in listening for connections" << std::endl;
        closesocket(server_socket);
        WSACleanup();
        return;
    }

    std::cout << "Server started. Waiting for connections..." << std::endl;

    while (true) {
        // Accept incoming connection
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket == INVALID_SOCKET) {
            std::cerr << "Error in accepting connection" << std::endl;
            continue;
        }

        // Handle client in a separate thread
        std::thread client_thread(client_handler, client_socket);
        client_thread.detach();
    }

    closesocket(server_socket);
    WSACleanup();
}

int main() {
    int port;
    std::cout << "Enter server port: ";
    std::cin >> port;

    run_server(port);

    return 0;
}
