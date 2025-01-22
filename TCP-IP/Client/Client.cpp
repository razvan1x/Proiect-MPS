// Client.cpp : Defines the entry point for the application.

#include "framework.h"
#include "Client.h"

#include <iostream>
#include <string>
#include <ws2tcpip.h>
#include <winsock2.h>
#include <thread>
#include <map>
#include <mutex>
//#include <vector>

#pragma comment(lib, "Ws2_32.lib")

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

int myID, targetID;
const int BUFFER_SIZE = 1024;

std::map<int, SOCKET> clients;
std::mutex clients_mutex;

SOCKET client_socket = INVALID_SOCKET;

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    Server(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    Client(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    PortDialog(HWND, UINT, WPARAM, LPARAM);
void run_client(const std::string&,int);
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
    LoadStringW(hInstance, IDC_CLIENT, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_CLIENT));

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
    return (int)msg.wParam;
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
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDC_CLIENT));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_CLIENT);
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

void RunClientInBackground(const std::string& server_ip, int port) {
    // Apelați funcția run_client în acest fir de execuție
    run_client(server_ip, port);
}

// Funcție pentru trimiterea unui mesaj împreună cu dimensiunea sa
void send_message(SOCKET client_socket, const std::string& message) {
    // Trimiteți întâi dimensiunea mesajului
    int message_size = message.size();
    send(client_socket, reinterpret_cast<char*>(&message_size), sizeof(message_size), 0);

    // Apoi trimiteți mesajul însuși
    send(client_socket, message.c_str(), message_size, 0);
}

/*void UpdateClientComboBox() {
    // Șterge toate elementele din combobox
    SendMessage(g_comboBox, CB_RESETCONTENT, 0, 0);

    // Adăugare opțiune "Server" în combobox
    SendMessage(g_comboBox, CB_ADDSTRING, 0, (LPARAM)L"Server");

    // Adăugare clienți cunoscuți de server în combobox
    for (auto& pair : clients) {
        if (pair.first != clientID) { // Excludem clientul curent din listă
            std::wstring clientIDStr = std::to_wstring(pair.first);
            SendMessage(g_comboBox, CB_ADDSTRING, 0, (LPARAM)clientIDStr.c_str());
        }
    }
}
*/

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        // Crearea controalelor grafice
        CreateWindowW(L"STATIC", L"IP:", WS_VISIBLE | WS_CHILD, 10, 10, 25, 25, hWnd, nullptr, hInst, nullptr);
        HWND ipTextBox = CreateWindowW(L"EDIT", nullptr, WS_VISIBLE | WS_CHILD | WS_BORDER | ES_READONLY, 35, 10, 125, 25, hWnd, reinterpret_cast<HMENU>(IDC_IPADDRESS1), hInst, nullptr);
        CreateWindowW(L"STATIC", L"Port:", WS_VISIBLE | WS_CHILD, 10, 40, 30, 25, hWnd, nullptr, hInst, nullptr);
        HWND portTextBox = CreateWindowW(L"EDIT", nullptr, WS_VISIBLE | WS_CHILD | WS_BORDER | ES_READONLY, 50, 40, 50, 25, hWnd, reinterpret_cast<HMENU>(IDC_EDIT1), hInst, nullptr);
        CreateWindowW(L"BUTTON", L"Connect", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 180, 40, 80, 25, hWnd, reinterpret_cast<HMENU>(IDOK), hInst, nullptr);

        HWND eventLogLabel = CreateWindowW(L"STATIC", L"Event Log(de sters*):", WS_VISIBLE | WS_CHILD, 10, 75, 200, 25, hWnd, nullptr, hInst, nullptr);
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

        // Adăugarea ID-ul serverului la lista drop-down
        const wchar_t* clientIDs[] = {L"Server",L"1" ,L"2" ,L"3" };
        for (const wchar_t* id : clientIDs) {
            SendMessage(comboBox, CB_ADDSTRING, 0, (LPARAM)id);
        }

        //var glob este atribuită cu cea locală pt run_client/servr
        g_eventLogTextBox = eventLogTextBox;
        g_receivedMessagesTextBox = receivedMessagesTextBox;
        g_sentMessagesTextBox = sentMessagesTextBox;
        g_MessageTextBox = MessageTextBox;
        g_comboBox = comboBox;

        break;
    }
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        switch (wmId)
        {
        case IDC_IPADDRESS1: // Caseta de port și ip
        {
            if (HIWORD(wParam) == EN_CHANGE) { // Verificăm dacă s-a schimbat conținutul casetei
                // Dezactivăm butonul de conectare
                HWND ipTextBox = GetDlgItem(hWnd, IDC_IPADDRESS1);
                HWND portTextBox = GetDlgItem(hWnd, IDC_EDIT1);
                int ipTextLength = GetWindowTextLength(ipTextBox);
                int portTextLength = GetWindowTextLength(portTextBox);
                HWND connectButton = GetDlgItem(hWnd, IDOK);
                EnableWindow(connectButton, ipTextLength == 0 && portTextLength == 0);

                /* // Obținem adresa IP și portul
                wchar_t portBuffer[64], ipBuffer[64];
                GetWindowText(portTextBox, portBuffer, 64);
                GetWindowText(ipTextBox, ipBuffer, 64);
                int port = _wtoi(portBuffer);               
                // Conversia adresei IP la format ASCII
                char ip[64];
                WideCharToMultiByte(CP_ACP, 0, ipBuffer, -1, ip, 64, NULL, NULL);
                ip[63] = '\0'; // Adăugăm un terminator nul la sfârșitul șirului de caractere convertit
                // Afisare adresa IP și portul într-un MessageBox
                std::string message = "IP: " + std::string(ip) + ", Port: " + std::to_string(port);
                MessageBoxA(NULL, message.c_str(), "IP și Port", MB_OK);
                //std::string ips = "192.168.1.2";
                run_client(ip, port);*/
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
                // Verifică dacă opțiunea "Server" este selectată
                int selectedIndex = SendMessage(g_comboBox, CB_GETCURSEL, 0, 0);
                if (selectedIndex != CB_ERR) {
                    wchar_t selectedText[256];
                    SendMessage(g_comboBox, CB_GETLBTEXT, selectedIndex, (LPARAM)selectedText);
                    std::string messageStr(messageToSend.begin(), messageToSend.end());
                    targetID = _wtoi(selectedText);
                    std::string target = std::to_string(targetID);
                    std::string myid = std::to_string(myID);
                    if (wcscmp(selectedText, L"Server") == 0) {
                        // Trimite mesajul la Server  
                        SOCKET serverSocket = client_socket;
                        send_message(serverSocket, messageStr);  
                        target = "0";
                        std::string mest = messageStr.c_str();
                        std::string wserver = std::string(mest.begin(), mest.end()) + "»" + target + "«" + myid;
                        send_message(serverSocket, wserver);
                    }                    
                    else {
                        SOCKET clientSocket = client_socket;                      
                        // Construiește mesajul care să conțină atât mesajul propriu-zis, cât și ID-ul clientului țintă
                        std::string fullMessage = std::string(messageToSend.begin(), messageToSend.end()) + "»" + target + "«" + myid;
                        // Trimiterea mesajului către server
                        send_message(clientSocket, fullMessage);
                    }
                }

                // Afișați mesajul trimis în caseta de mesaje trimise
                std::wstring sentMessage = L">Mesaj trimis: " + messageToSend + L"\r\n";
                SendMessage(g_sentMessagesTextBox, EM_REPLACESEL, FALSE, (LPARAM)sentMessage.c_str());

                // Goliți caseta MessageTextBox
                SetWindowText(g_MessageTextBox, L"");
            }
            else {
                // Opțional
                MessageBox(NULL, L"Scrie un mesaj mai întâi", L"Error", MB_OK);
            }

            break;
        }

        case IDOK:
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
            // Obțineți valoarea introdusă în casetele de editare pentru port, adresă IP și ID
            wchar_t portBuffer[64], ipBuffer[64];
            GetDlgItemText(hDlg, IDC_EDIT1, portBuffer, 64);
            GetDlgItemText(hDlg, IDC_IPADDRESS1, ipBuffer, 64);

            int port = _wtoi(portBuffer);
            
            // Conversia adresei IP la format ASCII
            char ip[64];
            WideCharToMultiByte(CP_ACP, 0, ipBuffer, -1, ip, 64, NULL, NULL);
            ip[63] = '\0'; // Adăugăm un terminator nul la sfârșitul șirului de caractere convertit
            std::string sa=ip;
            // Trimiteți valorile introduse înapoi la fereastra principală
            HWND hWndMain = GetParent(hDlg);
            if (hWndMain != nullptr)
            {
                // Adăugați valorile la casetele de text corespunzătoare din fereastra principală               
                HWND ipTextBox = GetDlgItem(hWndMain, IDC_IPADDRESS1);
                SendMessage(ipTextBox, EM_REPLACESEL, FALSE, (LPARAM)ipBuffer);

                HWND portTextBox = GetDlgItem(hWndMain, IDC_EDIT1);
                SendMessage(portTextBox, EM_REPLACESEL, FALSE, (LPARAM)portBuffer);

                // Închideți fereastra de dialog
                EndDialog(hDlg, IDOK);
        
                // Rulați serverul într-un fir de execuție separat
                std::thread server_thread(RunClientInBackground, sa, port);//"192.168.1.2"
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


void run_client(const std::string& server_ip, int server_port) {
    // Afișare mesaj cu adresa IP și portul
    const char* ip_c_str = server_ip.c_str();
    char message[256];
    sprintf_s(message, "Server IP: %s, Port: %d", ip_c_str, server_port);
    MessageBoxA(NULL, message, "Server Info", MB_OK);
 
    WSADATA wsData;
    //MessageBox(NULL, (LPARAM)server_ip, L"Error", MB_OK);
    if (WSAStartup(MAKEWORD(2, 2), &wsData) != 0) {
        MessageBox(NULL, L"Error in initializing Winsock", L"Error", MB_OK);
        return;
    }

    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    int bytes_received;

    // Create socket
    client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client_socket == INVALID_SOCKET) {
        MessageBox(NULL, L"Error in creating socket", L"Error", MB_OK);
        WSACleanup();
        return;
    }

    // Specify server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr);

    // Connect to server
    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        MessageBox(NULL, L"Error in connecting to server", L"Error", MB_OK);
        closesocket(client_socket);
        WSACleanup();
        return;
    }

    //MessageBox(NULL, L"Connected to server", L"Connected", MB_OK);
    SendMessage(g_eventLogTextBox, EM_REPLACESEL, FALSE, LPARAM(L">Connected to server\r\n"));

    int message_size;
    
    // id-ul propriu
    bytes_received = recv(client_socket, reinterpret_cast<char*>(&message_size), sizeof(message_size), 0);
    if (bytes_received <= 0) {
        if (bytes_received == 0) {
            // Serverul s-a deconectat
            MessageBox(NULL, L"Client disconnected", L"Server Info", MB_OK);
        }
        else {
            // Eroare la recepționarea dimensiunii mesajului de la server
            MessageBox(NULL, L"Error in receiving message size from client", L"Server Error", MB_OK);
        }
    }
    // Receive message from server
    bytes_received = recv(client_socket, buffer, message_size, 0);
    if (bytes_received <= 0) {
        if (bytes_received == 0) {
            // Serverul s-a deconectat
            MessageBox(NULL, L"Client disconnected", L"Server Info", MB_OK);
        }
        else {
            // Eroare la recepționarea datelor de la server
            MessageBox(NULL, L"Error in receiving data from client", L"Server Error", MB_OK);
        }
    }
    else {
        std::string me(buffer, bytes_received);
        std::string message = ">Conectat la Server cu ID-ul: " + me;
        std::wstring wideMessage(message.begin(), message.end());
        SendMessage(g_eventLogTextBox, EM_REPLACESEL, FALSE, LPARAM(wideMessage.c_str()));
        SendMessage(g_eventLogTextBox, EM_REPLACESEL, FALSE, LPARAM(L"\r\n"));
        myID = stoi(me);
    }


    // Communicate with server
    while (true) {
        // Receive message size from server
        //int message_size;
        bytes_received = recv(client_socket, reinterpret_cast<char*>(&message_size), sizeof(message_size), 0);
        if (bytes_received <= 0) {
            if (bytes_received == 0) {
                // Serverul s-a deconectat
                MessageBox(NULL, L"Client disconnected", L"Server Info", MB_OK);
            }
            else {
                // Eroare la recepționarea dimensiunii mesajului de la server
                MessageBox(NULL, L"Error in receiving message size from client", L"Server Error", MB_OK);
            }
            break;
        }

        // Receive message from server
        bytes_received = recv(client_socket, buffer, message_size, 0);
        if (bytes_received <= 0) {
            if (bytes_received == 0) {
                // Serverul s-a deconectat
                MessageBox(NULL, L"Client disconnected", L"Server Info", MB_OK);
            }
            else {
                // Eroare la recepționarea datelor de la server
                MessageBox(NULL, L"Error in receiving data from client", L"Server Error", MB_OK);
            }
            break;
        }
        else {          
            std::string mes(buffer, bytes_received);
            std::string message = ">Mesaj primit: " + mes;
            std::wstring wideMessage(message.begin(), message.end());
            SendMessage(g_receivedMessagesTextBox, EM_REPLACESEL, FALSE, LPARAM(wideMessage.c_str()));
            SendMessage(g_receivedMessagesTextBox, EM_REPLACESEL, FALSE, LPARAM(L"\r\n"));                          
        }
    }
    
    closesocket(client_socket);
    WSACleanup();
}
