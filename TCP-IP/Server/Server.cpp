// WindowsProject1.cpp : Defines the entry point for the application.

#include "framework.h"
#include "WindowsProject1.h"

#include <iostream>
#include <string>
#include <ws2tcpip.h>
#include <winsock2.h>
#include <thread>
#include <map>
#include <mutex>
//#include <vector>

#pragma comment(lib, "Ws2_32.lib")

#pragma warning(disable : 4996)

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

HWND g_eventLogTextBox = nullptr;
HWND g_receivedMessagesTextBox = nullptr;
HWND g_sentMessagesTextBox = nullptr;
HWND g_MessageTextBox = nullptr;
HWND g_comboBox = nullptr;

const int BUFFER_SIZE = 1024;

std::map<int, SOCKET> clients;
std::mutex clients_mutex;

int client_id = 0;

// Server variables
SOCKET client_socket = INVALID_SOCKET;
SOCKET server_socket = INVALID_SOCKET;

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    Server(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    Client(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    PortDialog(HWND, UINT, WPARAM, LPARAM);

void run_server(int);
void client_handler(SOCKET);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_WINDOWSPROJECT1, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_WINDOWSPROJECT1));

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return (int) msg.wParam;
}


ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDC_WINDOWSPROJECT1));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_WINDOWSPROJECT1);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance; // Store instance handle in our global variable

    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
    {
        return FALSE;
    }

    // Set the desired width and height for the window
    int desiredWidth = 300;
    int desiredHeight = 700;

    // Calculate the position for centering the window
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int xPos = (screenWidth - desiredWidth) / 2;
    int yPos = (screenHeight - desiredHeight) / 2;

    // Set the window position and size
    SetWindowPos(hWnd, HWND_TOP, xPos, yPos, desiredWidth, desiredHeight, SWP_SHOWWINDOW);


    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return TRUE;
}

// Funcție pentru a obține IP-ul dispozitivului local
std::string GetLocalIPAddress() {
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        std::cerr << "WSAStartup failed: " << iResult << std::endl;
        return "WSAStartup err";
    }

    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == SOCKET_ERROR) {
        std::cerr << "Error getting hostname: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return "hostname err";
    }

    struct addrinfo* result = nullptr;
    struct addrinfo hints;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;  // Use IPv4
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    iResult = getaddrinfo(hostname, nullptr, &hints, &result);
    if (iResult != 0) {
        std::cerr << "getaddrinfo failed: " << iResult << std::endl;
        WSACleanup();
        return "getaddrinfo err";
    }

    struct sockaddr_in* addr_in = (struct sockaddr_in*)result->ai_addr;
    char ipstr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr_in->sin_addr, ipstr, sizeof(ipstr));
    std::cout << "IP Address: " << ipstr << std::endl;

    freeaddrinfo(result);
    WSACleanup();
    return ipstr;
}

void RunServerInBackground(int port) {
    // Apelați funcția run_server în acest fir de execuție
    run_server(port);
}

// Funcție pentru trimiterea unui mesaj împreună cu dimensiunea sa
void send_message(SOCKET client_socket, const std::string& message) {
    // Trimiteți întâi dimensiunea mesajului
    int message_size = message.size();
    send(client_socket, reinterpret_cast<char*>(&message_size), sizeof(message_size), 0);

    // Apoi trimiteți mesajul însuși
    send(client_socket, message.c_str(), message_size, 0);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        // Crearea controalelor grafice
        CreateWindowW(L"STATIC", L"IP:", WS_VISIBLE | WS_CHILD, 10, 10, 25, 25, hWnd, nullptr, hInst, nullptr);
        HWND ipTextBox = CreateWindowW(L"EDIT", nullptr, WS_VISIBLE | WS_CHILD | WS_BORDER | ES_READONLY, 35, 10, 125, 25, hWnd, nullptr, hInst, nullptr);
        CreateWindowW(L"STATIC", L"Port:", WS_VISIBLE | WS_CHILD, 10, 40, 30, 25, hWnd, nullptr, hInst, nullptr);
        HWND portTextBox = CreateWindowW(L"EDIT", nullptr, WS_VISIBLE | WS_CHILD | WS_BORDER | ES_READONLY, 50, 40, 50, 25, hWnd, reinterpret_cast<HMENU>(IDC_EDIT1), hInst, nullptr);
        CreateWindowW(L"BUTTON", L"Connect", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 180, 40, 80, 25, hWnd, reinterpret_cast<HMENU>(IDOK), hInst, nullptr);

        HWND eventLogLabel = CreateWindowW(L"STATIC", L"Event Log:", WS_VISIBLE | WS_CHILD, 10, 75, 200, 25, hWnd, nullptr, hInst, nullptr);
        HWND eventLogTextBox = CreateWindowW(L"EDIT", nullptr, WS_VISIBLE | WS_CHILD | WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY, 10, 105, 250, 100, hWnd, nullptr, hInst, nullptr);
        
        HWND receivedMessagesLabel = CreateWindowW(L"STATIC", L"Received Messages:", WS_VISIBLE | WS_CHILD, 10, 225, 200, 25, hWnd, nullptr, hInst, nullptr);
        HWND receivedMessagesTextBox = CreateWindowW(L"EDIT", nullptr, WS_VISIBLE | WS_CHILD | WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY, 10, 255, 250, 100, hWnd, nullptr, hInst, nullptr);

        HWND sentMessagesLabel = CreateWindowW(L"STATIC", L"Sent Messages:", WS_VISIBLE | WS_CHILD, 10, 365, 200, 25, hWnd, nullptr, hInst, nullptr);
        HWND sentMessagesTextBox = CreateWindowW(L"EDIT", nullptr, WS_VISIBLE | WS_CHILD | WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY, 10, 395, 250, 100, hWnd, nullptr, hInst, nullptr);

        HWND MessageLabel = CreateWindowW(L"STATIC", L"Message:", WS_VISIBLE | WS_CHILD, 10, 545, 200, 25, hWnd, nullptr, hInst, nullptr);
        HWND MessageTextBox = CreateWindowW(L"EDIT", nullptr, WS_VISIBLE | WS_CHILD | WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL, 10, 570, 250, 25, hWnd, nullptr, hInst, nullptr);

        CreateWindowW(L"BUTTON", L"Transmit", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 10, 600, 80, 25, hWnd, reinterpret_cast<HMENU>(IDC_BUTTON1), hInst, nullptr);

        HWND comboBox = CreateWindowW(L"COMBOBOX", nullptr, WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST, 92.5, 600, 87.5, 100, hWnd, nullptr, hInst, nullptr);

        CreateWindowW(L"BUTTON", L"Quit", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 180, 600, 80, 25, hWnd, reinterpret_cast<HMENU>(IDCANCEL), hInst, nullptr);

        // Adăugare font pentru casetele text
        HFONT hFont = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Arial");     
        SendMessage(eventLogTextBox, WM_SETFONT, WPARAM(hFont), TRUE);
        SendMessage(receivedMessagesTextBox, WM_SETFONT, WPARAM(hFont), TRUE);
        SendMessage(sentMessagesTextBox, WM_SETFONT, WPARAM(hFont), TRUE);
        SendMessage(MessageTextBox, WM_SETFONT, WPARAM(hFont), TRUE);
        
        // Adăugarea ID-urilor clienților la lista drop-down
        const wchar_t* clientIDs[] = { L"Broadcast" };
        for (const wchar_t* id : clientIDs) {
            SendMessage(comboBox, CB_ADDSTRING, 0, (LPARAM)id);
        }

        // Obțineți IP-ul local
        std::string localIP = GetLocalIPAddress();
        std::wstring wideLocalIP(localIP.begin(), localIP.end());
        const wchar_t* wideIP = wideLocalIP.c_str();

        g_eventLogTextBox = eventLogTextBox;
        g_receivedMessagesTextBox = receivedMessagesTextBox;
        g_sentMessagesTextBox = sentMessagesTextBox;
        g_MessageTextBox = MessageTextBox;
        g_comboBox = comboBox;

        // Afișați IP-ul în caseta text
        SetWindowTextW(ipTextBox, wideIP);

        break;
    }
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);

        switch (wmId)
        {
        case IDC_EDIT1: // Caseta de port
        {
            if (HIWORD(wParam) == EN_CHANGE) { // Verificăm dacă s-a schimbat conținutul casetei
                // Dezactivăm butonul de conectare dacă caseta de port nu este goală
                HWND portTextBox = GetDlgItem(hWnd, IDC_EDIT1);
                int textLength = GetWindowTextLength(portTextBox);
                HWND connectButton = GetDlgItem(hWnd, IDOK);
                EnableWindow(connectButton, textLength == 0);
            }
            break;
        }

        case IDC_BUTTON1: //Butonul transmit
        {
            // Obțineți textul din caseta MessageTextBox
            wchar_t messageBuffer[256];
            GetWindowText(g_MessageTextBox, messageBuffer, 256);
            std::wstring messageToSend(messageBuffer);

            // Verificați dacă caseta MessageTextBox nu este goală
            if (!messageToSend.empty()) {
                // Verifică dacă opțiunea "Broadcast" este selectată
                int selectedIndex = SendMessage(g_comboBox, CB_GETCURSEL, 0, 0);

                wchar_t selectedText[256];
                SendMessage(g_comboBox, CB_GETLBTEXT, selectedIndex, (LPARAM)selectedText);
                std::string messageStr(messageToSend.begin(), messageToSend.end());
                int clientID = _wtoi(selectedText);
                std::string clientIDstr = std::to_string(clientID);

                if (selectedIndex != CB_ERR) {            
                    if (wcscmp(selectedText, L"Broadcast") == 0) {
                        // Trimite mesajul la toți clienții conectați
                        for (auto& pair : clients) {
                            std::string mess="De la Server: " + std::string(messageToSend.begin(), messageToSend.end());
                            send_message(pair.second, mess);
                        }
                    }
                    else {
                        // Verifică dacă textul selectat corespunde unui id de client         
                        auto clientSocketIterator = clients.find(clientID);
                        if (clientSocketIterator != clients.end()) {
                            // Clientul cu id-ul specificat a fost găsit                
                            SOCKET clientSocket = clientSocketIterator->second;
                            std::string mess = "De la Server: " + std::string(messageToSend.begin(), messageToSend.end());
                            send_message(clientSocket, mess);
                        }
                        else {
                            MessageBox(NULL, L"Id-ul clientului nu a fost găsit", L"Error", MB_OK);
                        }
                    }
                }

                // Afișați mesajul trimis în caseta de mesaje trimise
                std::wstring str(clientIDstr.begin(), clientIDstr.end());
                if (str == L"0") {
                    str = L"";
                    std::wstring sentMessage = L">Mesaj trimis spre toți Clienții: " + messageToSend + L"\r\n";
                    SendMessage(g_sentMessagesTextBox, EM_REPLACESEL, FALSE, (LPARAM)sentMessage.c_str());
                } 
                else {
                    std::wstring sentMessage = L">Mesaj trimis spre Clientul " + str + L": " + messageToSend + L"\r\n";
                    SendMessage(g_sentMessagesTextBox, EM_REPLACESEL, FALSE, (LPARAM)sentMessage.c_str());
                }             
                // Goliți caseta MessageTextBox
                SetWindowText(g_MessageTextBox, L"");
            }
            else {
                // Opțional
                MessageBox(NULL, L"Scrie un mesaj mai întâi", L"Error", MB_OK);
            }

            break;
        }

        case IDOK: //Butonul de conectare
        {
            DialogBox(hInst, MAKEINTRESOURCE(IDD_DIALOG3), hWnd, PortDialog);       
            break;
        }

        case IDCANCEL: // IDCANCEL este identificatorul asociat butonului "Quit"
        {
            // Închideți fereastra principală și terminați aplicația
            DestroyWindow(hWnd);
            break;
        }

        }
        break;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        EndPaint(hWnd, &ps);
        break;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for port dialog box.
INT_PTR CALLBACK PortDialog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
        {
            // Obțineți valoarea introdusă în caseta de editare
            wchar_t buffer[64];
            GetDlgItemText(hDlg, IDC_EDIT1, buffer, 64);
            int port = _wtoi(buffer);

            // Trimiteți valoarea introdusă înapoi la fereastra principală
            HWND hWndMain = GetParent(hDlg);
            if (hWndMain != nullptr)
            {
                // Adăugați valoarea la caseta de text "Event Log" a ferestrei principale
                HWND portTextBox = GetDlgItem(hWndMain, IDC_EDIT1);
                SendMessage(portTextBox, EM_REPLACESEL, FALSE, (LPARAM)buffer);

                // Închideți fereastra de dialog
                EndDialog(hDlg, IDOK);

                // Rulați serverul într-un fir de execuție separat
                std::thread server_thread(RunServerInBackground,port);
                server_thread.detach();
            }
            break;
        }
        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            break;
        }
        break;

    default:
        return FALSE;
    }
    return TRUE;
}

void run_server(int port) {
    WSADATA wsData;
    if (WSAStartup(MAKEWORD(2, 2), &wsData) != 0) {
        MessageBoxW(NULL, L"Error in initializing Winsock", L"Server Error", MB_OK);
        return;
    }

    //SOCKET server_socket;
    struct sockaddr_in server_addr, client_addr;
    int client_len = sizeof(client_addr);

    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == INVALID_SOCKET) {
        MessageBoxW(NULL, L"Error in creating socket", L"Server Error", MB_OK);
        WSACleanup();
        return;
    }

    std::string localIP = GetLocalIPAddress();
    const char* ip = localIP.c_str();
    std::wstring wideLocalIP(localIP.begin(), localIP.end());
    const wchar_t* wideIP = wideLocalIP.c_str();

    // Specify server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip); // Adresa IP specifică a interfeței, altfel: INADDR_ANY;
    server_addr.sin_port = htons(port);

    // Bind socket
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        int bindError = WSAGetLastError();
        wchar_t errorMsg[256];
        swprintf(errorMsg, L"Error in binding socket: %d", bindError);
        MessageBoxW(NULL, errorMsg, L"Error", MB_OK);
        closesocket(server_socket);
        WSACleanup();
        return;
    }

    // Get server IP address after binding
    struct sockaddr_in server_info;
    int server_info_len = sizeof(server_info);
    if (getsockname(server_socket, (struct sockaddr*)&server_info, &server_info_len) == SOCKET_ERROR) {
        MessageBoxW(NULL, L"Error getting server address", L"Server Error", MB_OK);
        closesocket(server_socket);
        WSACleanup();
        return;
    }

    // Convert server IP address to wide character string
    wchar_t ipstr[INET_ADDRSTRLEN];
    InetNtop(AF_INET, &server_info.sin_addr, ipstr, INET_ADDRSTRLEN);

    // Listen for incoming connections
    if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR) {
        MessageBoxW(NULL, L"Error in listening for connections", L"Server Error", MB_OK);
        closesocket(server_socket);
        WSACleanup();
        return;
    }

    std::string portString = std::to_string(port);
    std::string message = ">Serverul a pornit pe portul:" + portString + " cu IP-ul:";
    std::wstring wideMessage(message.begin(), message.end());

    SendMessage(g_eventLogTextBox, EM_REPLACESEL, FALSE, (LPARAM)wideMessage.c_str());
    SendMessage(g_eventLogTextBox, EM_REPLACESEL, FALSE, (LPARAM)ipstr);

    // Set up a set of file descriptors for select
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(server_socket, &readfds);
    int max_fd = server_socket;

    while (true) {

        // Set a timeout of 0 seconds and 0 microseconds
        struct timeval timeout;
        timeout.tv_sec = 30;
        timeout.tv_usec = 1;

        // Wait indefinitely for activity on any of the file descriptors
        fd_set tmp_fds = readfds;
        int activity = select(max_fd + 1, &tmp_fds, nullptr, nullptr, &timeout);
        if (activity == SOCKET_ERROR) {
            MessageBoxW(NULL, L"Error in activity listening", L"Server Error", MB_OK);
            break;
        }
        
        // Check if there is a new connection request
        if (FD_ISSET(server_socket, &tmp_fds)) {
            // Accept incoming connection
            client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
            if (client_socket == INVALID_SOCKET) {
                MessageBoxW(NULL, L"Error in accepting connection", L"Server Error", MB_OK);
                break;
            }

            // Add the new client socket to the set
            FD_SET(client_socket, &readfds);
            if (client_socket > max_fd) {
                max_fd = client_socket;
            }
        
            // Handle client in a separate thread
            std::thread client_thread(client_handler, client_socket);
            client_thread.detach(); 
           
        }
    }
    
    closesocket(server_socket);
    WSACleanup();
}

void client_handler(SOCKET client_socket) {
    char buffer[BUFFER_SIZE];
    int bytes_received;
    wchar_t wbuffer[10];

    // Creare ID automat
    client_id++;// inițiat la 0, devinee 1 pt primul client

    // Semnalare în eventBox
    std::string id = std::to_string(client_id);   
    std::string message = ">Un client sa conectat cu ID-ul:" + id + ".";
    std::wstring wideMessage(message.begin(), message.end());
    SendMessage(g_eventLogTextBox, EM_REPLACESEL, FALSE, LPARAM(L"\r\n"));
    SendMessage(g_eventLogTextBox, EM_REPLACESEL, FALSE, (LPARAM)wideMessage.c_str());

    // Adăugarea ID-urilor clienților la lista drop-down       
    std::string wideid(id.begin(), id.end());
    SendMessage(g_comboBox, CB_ADDSTRING, 0, (LPARAM)wideid.c_str());
    
    // Notify server about client's connection
    sockaddr_in client_info;
    int len = sizeof(client_info);
    if (getpeername(client_socket, (sockaddr*)&client_info, &len) == 0) {
        char client_ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_info.sin_addr), client_ip_str, INET_ADDRSTRLEN);
        int client_port_num = ntohs(client_info.sin_port);
    }
    
    //trimite id-ul propriu
    send_message(client_socket,id);

    // Adaugă clientul în map-ul clients împreună cu ID-ul său
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        clients[client_id] = client_socket;
    }

    while (true) {
        // Receive message size from client
        int message_size;
        bytes_received = recv(client_socket, reinterpret_cast<char*>(&message_size), sizeof(message_size), 0);
        if (bytes_received <= 0) {
            if (bytes_received == 0) {
                // Clientul s-a deconectat
                MessageBox(NULL, L"Client disconnected", L"Server Info", MB_OK);
            }
            else {
                // Eroare la recepționarea dimensiunii mesajului de la client
                MessageBox(NULL, L"Error in receiving message size from client", L"Server Error", MB_OK);
            }
            break;
        }

        // Receive message from client
        bytes_received = recv(client_socket, buffer, message_size, 0);
        if (bytes_received <= 0) {
            if (bytes_received == 0) {
                // Clientul s-a deconectat
                MessageBox(NULL, L"Client disconnected", L"Server Info", MB_OK);
            }
            else {
                // Eroare la recepționarea datelor de la client
                MessageBox(NULL, L"Error in receiving data from client", L"Server Error", MB_OK);
            }
            break;
        }
        else {
            std::string callerIDStr = "";
            std::string mes(buffer, bytes_received);
            size_t separatorPos1 = mes.find('»'); 
            size_t separatorPos2 = mes.find('«');
            if (separatorPos1 != std::string::npos && separatorPos2 != std::string::npos) {
                std::string message = mes.substr(0, separatorPos1);
                std::string target = mes.substr(separatorPos1 + 1, separatorPos2 - separatorPos1 - 1);
                callerIDStr = mes.substr(separatorPos2 + 1);
                // Convertim id-ul clientului apelant la întreg
                int callerClientID = std::stoi(callerIDStr);
                int targetClientID = std::stoi(target);
                // Trimite mesajul către clientul țintă
                auto clientSocketIterator = clients.find(targetClientID);
                if (clientSocketIterator != clients.end()) {
                    SOCKET clientSocket = clientSocketIterator->second;
                    std::string strMessage = ">Clientul "+ callerIDStr +" apeleaza clientul "+ target;
                    std::wstring wideMessage(strMessage.begin(), strMessage.end());
                    SendMessage(g_eventLogTextBox, EM_REPLACESEL, FALSE, LPARAM(L"\r\n"));
                    SendMessage(g_eventLogTextBox, EM_REPLACESEL, FALSE, (LPARAM)wideMessage.c_str());
                    std::string mesaj ="Mesaj de la Clientul "+ callerIDStr + ": "+ std::string(message.begin(), message.end());
                    send_message(clientSocket, mesaj);
                }
                else if (targetClientID == 0) {
                    std::string mesaj = ">Mesaj primit de la Clientul "+ callerIDStr +": " + message;            
                    std::wstring wideMessage(mesaj.begin(), mesaj.end());           
                    SendMessage(g_receivedMessagesTextBox, EM_REPLACESEL, FALSE, LPARAM(wideMessage.c_str()));
                    SendMessage(g_receivedMessagesTextBox, EM_REPLACESEL, FALSE, LPARAM(L"\r\n"));
                }
                else {
                    MessageBox(NULL, L"Id-ul clientului nu a fost găsit", L"Error", MB_OK);
                }
            }                     
        }      
    }
    
    // Remove client from the map
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        clients.erase(client_id);
    }

    // Închideți socketul asociat cu clientul deconectat
    closesocket(client_socket);
    WSACleanup();
}