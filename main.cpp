#include <iostream>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <thread>
#include <fstream>
#include <string>
#include <map>
#include <vector>

using namespace std;

#pragma comment(lib,"ws2_32.lib")

map<string, string> peerList; 

bool Initialize() {
    WSADATA data;
    return WSAStartup(MAKEWORD(2, 2), &data) == 0;
}

void BroadcastPresence(const string& peerName) {
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in broadcastAddr;
    broadcastAddr.sin_family = AF_INET;
    broadcastAddr.sin_port = htons(54000);
    inet_pton(AF_INET, "255.255.255.255", &broadcastAddr.sin_addr);

    int broadcast = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char*)&broadcast, sizeof(broadcast));

    while (true) {
        string message = "PEER:" + peerName;
        sendto(sock, message.c_str(), message.size() + 1, 0, (sockaddr*)&broadcastAddr, sizeof(broadcastAddr));
        this_thread::sleep_for(chrono::seconds(5));
    }
    closesocket(sock);
}

void ListenForPeers() {
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in recvAddr;
    recvAddr.sin_family = AF_INET;
    recvAddr.sin_port = htons(54000);
    recvAddr.sin_addr.s_addr = INADDR_ANY;

    bind(sock, (sockaddr*)&recvAddr, sizeof(recvAddr));

    char buffer[4096];
    sockaddr_in peerAddr;
    int peerAddrLen = sizeof(peerAddr);

    while (true) {
        int bytesIn = recvfrom(sock, buffer, 4096, 0, (sockaddr*)&peerAddr, &peerAddrLen);
        if (bytesIn == SOCKET_ERROR) continue;

        string message = string(buffer, bytesIn);
        if (message.find("PEER:") == 0) {
            string peerName = message.substr(5);
            char peerIP[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(peerAddr.sin_addr), peerIP, INET_ADDRSTRLEN);
            peerList[peerIP] = peerName;
            cout << "Discovered peer: " << peerName << " at " << peerIP << endl;
        }
    }
    closesocket(sock);
}

void ShowDiscoveredPeers() {
    cout << "\nDiscovered Peers:\n";
    int index = 1;
    for (const auto& peer : peerList) {
        cout << "   " << index << ". " << peer.second << " at " << peer.first << endl;
        index++;
    }
}

void ConnectAndSendFile(const string& peerIP, const string& filename) {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in peerAddr;
    peerAddr.sin_family = AF_INET;
    peerAddr.sin_port = htons(12345);
    inet_pton(AF_INET, peerIP.c_str(), &peerAddr.sin_addr);

    if (connect(sock, (sockaddr*)&peerAddr, sizeof(peerAddr)) == SOCKET_ERROR) {
        cout << "Failed to connect to peer at " << peerIP << endl;
        closesocket(sock);
        return;
    }

    ifstream file(filename, ios::binary);  
    if (!file.is_open()) {
        cout << "Failed to open file: " << filename << endl;
        closesocket(sock);
        return;
    }

    // Read and send the file
    char buffer[4096];
    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
        send(sock, buffer, file.gcount(), 0);  
    }

    cout << "File '" << filename << "' sent successfully to " << peerIP << "!" << endl;

    file.close();
    closesocket(sock);
}

void ListenForIncomingConnections(int port) {
    SOCKET listener = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    bind(listener, (sockaddr*)&serverAddr, sizeof(serverAddr));
    listen(listener, SOMAXCONN);

    cout << "Server is listening on port " << port << "...\n";

    SOCKET clientSocket = accept(listener, nullptr, nullptr);
    if (clientSocket != INVALID_SOCKET) {
        ofstream outFile("received_file", ios::binary);  
        if (!outFile.is_open()) {
            cout << "Failed to create file for saving received data!\n";
            closesocket(clientSocket);
            return;
        }

        char buffer[4096];
        int bytesReceived = 0;
        while ((bytesReceived = recv(clientSocket, buffer, 4096, 0)) > 0) {
            outFile.write(buffer, bytesReceived); 
        }

        cout << "File received and saved as 'received_file'.\n";

        outFile.close();
        closesocket(clientSocket);
    }
    closesocket(listener);
}

void StartClient() {
    if (peerList.empty()) {
        cout << "No peers discovered. Try discovery mode first.\n";
        return;
    }

    ShowDiscoveredPeers();
    cout << "Enter the peer number to send the file to: ";
    int choice;
    cin >> choice;
    cin.ignore(); 

    auto it = peerList.begin();
    advance(it, choice - 1);
    string peerIP = it->first;

    bool sendMore = true;
    while (sendMore) {
        cout << "Enter the file name to send: ";
        string filename;
        getline(cin, filename);

        ConnectAndSendFile(peerIP, filename);

       
        char anotherFile;
        cout << "Do you want to send another file? (y/n): ";
        cin >> anotherFile;
        cin.ignore();

        if (anotherFile != 'y') {
            sendMore = false;
        }
    }
}

void StartServer() {
    cout << "Enter port number to listen on: ";
    int port;
    cin >> port;
    cin.ignore();

    ListenForIncomingConnections(port);
}

void DiscoveryMode() {
    cout << "Enter your peer name: ";
    string peerName;
    getline(cin, peerName);

    
    thread broadcastThread(BroadcastPresence, peerName);
    thread listenThread(ListenForPeers);

    bool running = true;
    while (running) {
        cout << "\nChoose an action:\n";
        cout << "1. Continue discovering peers\n";
        cout << "2. Switch to server mode to receive a file\n";
        cout << "3. Switch to client mode to send a file\n";
        cout << "Enter your choice: ";

        int choice;
        cin >> choice;
        cin.ignore(); 

        switch (choice) {
        case 1:
            cout << "Continuing discovery mode...\n";
            break;
        case 2:
            cout << "Switching to server mode...\n";
            StartServer();
            running = false;
            break;
        case 3:
            cout << "Switching to client mode...\n";
            StartClient();
            running = false;
            break;
        default:
            cout << "Invalid choice. Try again.\n";
        }
    }

    broadcastThread.join();
    listenThread.join();
}

int main() {
    if (!Initialize()) {
        cout << "Failed to initialize Winsock!" << endl;
        return 1;
    }

    while (true) {
        cout << "Enter mode (s for server, c for client, d for discovery): ";
        char mode;
        cin >> mode;
        cin.ignore(); 

        if (mode == 's') {
            StartServer();
        }
        else if (mode == 'c') {
            StartClient();
        }
        else if (mode == 'd') {
            DiscoveryMode();
        }
        else {
            cout << "Invalid mode. Please enter 's', 'c', or 'd'.\n";
        }
    }

    WSACleanup();
    return 0;
}

