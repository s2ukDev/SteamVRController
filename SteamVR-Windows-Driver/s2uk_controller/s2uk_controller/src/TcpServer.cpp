#include "TcpServer.h"
#include "VRLog.h"


SOCKET tcpSocket;
int port;
WSADATA wsaData;
int wsaerr;
WORD wVersionRequested;
bool running;

std::mutex clientsMutex;
std::vector<SOCKET> clients;

std::mutex msgMutex;
std::condition_variable msgCv;

// Helpers
static inline void rtrim(std::string& s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
}

void TcpSocketClass::enqueueMessage(SOCKET sock, const std::string& msg) {
    std::lock_guard<std::mutex> lockGuard(msgMutex);
    TcpSocketClass::msgQueue.push({ sock, msg });
    // LOG("enqueueMessage -> queue size = %zu", msgQueue.size());
    msgCv.notify_one();
}
// ------

void TcpSocketClass::handleClient(SOCKET clientSock) {
    char buf[2048];

    // Receive init message
    int byteCount = recv(clientSock, buf, sizeof(buf) - 1, 0);
    if (byteCount <= 0) { closesocket(clientSock); return; }
    buf[byteCount] = '\0';
    std::string firstMsg(buf, byteCount);
    rtrim(firstMsg);

    if (firstMsg != CLIENT_CONNECTION_MESSAGE) {
        closesocket(clientSock);
        return;
    }

    LOG("Client accepted, starting receive thread.");

    setsockopt(clientSock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&CLIENT_TIMEOUT, sizeof(CLIENT_TIMEOUT));

    std::thread([this, clientSock]() {
        char buf[2048];
        while (running) {
            int byteCount = recv(clientSock, buf, sizeof(buf) - 1, 0);

            if (byteCount == 0) {
                LOG("Client disconnected.");
                break;
            }
            else if (byteCount == SOCKET_ERROR) {
                int err = WSAGetLastError();
                if (err == WSAETIMEDOUT) {
                    LOG("Client inactive for 30 seconds. Deleting.");
                }
                else {
                    LOG("recv() failed with error: %d", err);
                }
                break;
            }


            sendQueuedMessages();

            buf[byteCount] = '\0';
            std::string msg(buf, byteCount);
            rtrim(msg);

            this->enqueueMessage(clientSock, msg);
            //LOG("Received: %s", msg.c_str());
        }

        closesocket(clientSock);
        {
            std::lock_guard<std::mutex> lockGuard(clientsMutex);
            clients.erase(std::remove(clients.begin(), clients.end(), clientSock), clients.end());
        }
        {
            std::lock_guard<std::mutex> lockGuard(outgoingMutex);
            outgoingMessages.erase(clientSock);
        }
        
        }).detach();
}

void TcpSocketClass::Connect(int port_) {
    this->port = port_;
    wVersionRequested = MAKEWORD(2, 2);
    wsaerr = WSAStartup(wVersionRequested, &wsaData);
    if (wsaerr != 0) { LOG("Winsock dll not found!"); return; }

    tcpSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (tcpSocket == INVALID_SOCKET) { LOG("Error at socket()"); WSACleanup(); return; }

    sockaddr_in service{};
    service.sin_family = AF_INET;
    service.sin_addr.s_addr = INADDR_ANY;
    service.sin_port = htons(port);
    if (bind(tcpSocket, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR) { LOG("bind() failed"); closesocket(tcpSocket); WSACleanup(); return; }

    if (listen(tcpSocket, SOMAXCONN) == SOCKET_ERROR) { LOG("listen() failed"); closesocket(tcpSocket); WSACleanup(); return; }

    running = true;

    std::thread([this]() {
        while (running) {
            sockaddr_in clientInfo{};
            int clientSz = sizeof(clientInfo);
            SOCKET clientSock = accept(tcpSocket, (SOCKADDR*)&clientInfo, &clientSz);
            if (clientSock == INVALID_SOCKET) {
                int err = WSAGetLastError();
                if (err == WSAEINTR || err == WSAEWOULDBLOCK) {
                    LOG("ClientSocket Error.");
                    continue;
                }
                LOG("accept() failed with error: %d", err);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }


            LOG("Client connected.");
            {
                std::lock_guard<std::mutex> lockGuard(clientsMutex);
                clients.push_back(clientSock);
            }

            std::thread t(&TcpSocketClass::handleClient, this, clientSock);
            t.detach();
        }
        }).detach();

}

void TcpSocketClass::broadcastMessage(const std::string& msg) {
    std::string m = msg;
    if (!m.empty() && m.back() != '\n') m.push_back('\n');

    std::vector<SOCKET> snapshot;
    {
        std::lock_guard<std::mutex> lockGuard(clientsMutex);
        snapshot = clients;
    }

    std::lock_guard<std::mutex> lockGuard(outgoingMutex);
    for (SOCKET s : snapshot) {
        outgoingMessages[s].emplace_back(m);
    }
}

bool TcpSocketClass::sendMessagesToClient(SOCKET clientSock) {
    std::lock_guard<std::mutex> lockGuard(outgoingMutex);

    auto it = outgoingMessages.find(clientSock);
    if (it == outgoingMessages.end() || it->second.empty()) return true; // nothing to send

    auto& queue = it->second;

    while (!queue.empty() && running) {
        OutMsg& m = queue.front();
        const char* ptr = m.data.data() + m.offset;
        int remaining = static_cast<int>(m.data.size() - m.offset);
        if (remaining <= 0) { queue.pop_front(); continue; }

        int sent = send(clientSock, ptr, remaining, 0);
        if (sent == SOCKET_ERROR) {
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) {
                return true;
            }
            LOG("send() failed with error: %d", err);
            return false;
        }

        if (sent == 0) {
            return false;
        }

        m.offset += static_cast<size_t>(sent);
        if (m.offset >= m.data.size()) {
            queue.pop_front();
        }
        else {
            return true;
        }
    }

    return true;
}

void TcpSocketClass::sendQueuedMessages() {
    std::vector<SOCKET> snapshot;
    {
        std::lock_guard<std::mutex> lockGuard(clientsMutex);
        snapshot = clients; // copy
    }

    for (SOCKET s : snapshot) {
        bool ok = sendMessagesToClient(s);
        if (!ok) {
            LOG("sendQueuedMessages: failed -> closing socket %llu", (unsigned long long)s);
            closesocket(s);
            std::lock_guard<std::mutex> lockGuard(clientsMutex);
            clients.erase(std::remove(clients.begin(), clients.end(), s), clients.end());
            std::lock_guard<std::mutex> lg2(outgoingMutex);
            outgoingMessages.erase(s);
        }
    }
}

bool TcpSocketClass::GetStatus() {
    return running;
}

void TcpSocketClass::Receive(char* outStr, int maxLen) {
    std::unique_lock<std::mutex> ul(msgMutex);

    msgCv.wait(ul, [&] { return !msgQueue.empty(); });

    ClientMessage cm = msgQueue.front();
    msgQueue.pop();
    strncpy_s(outStr, maxLen, cm.msg.c_str(), _TRUNCATE);
    outStr[maxLen - 1] = '\0';
}

void TcpSocketClass::CloseSocket() {
    running = false;
    closesocket(tcpSocket);

    std::lock_guard<std::mutex> lockGuard(clientsMutex);
    for (auto c : clients) closesocket(c);
    clients.clear();

    WSACleanup();
}