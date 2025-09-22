
#pragma once
#ifndef TcpServer_H
#define TcpServer_H

//#include <iostream>
#include <mutex>
#include <vector>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")

#include <queue>
#include <unordered_map>

class TcpSocketClass {
	SOCKET tcpSocket = 0, acceptSocket = 0;
	int port = 0;
	WSADATA wsaData;
	int wsaerr = 0;
	WORD wVersionRequested = 0;

	std::mutex clientsMutex;
	std::vector<SOCKET> clients;

	struct ClientMessage {
		SOCKET sock;
		std::string msg;
	};

	struct OutMsg {
		std::string data;
		size_t offset = 0;
		OutMsg() = default;
		OutMsg(std::string d) : data(std::move(d)), offset(0) {}
	};

	std::mutex msgMutex;
	std::queue<ClientMessage> msgQueue;
	std::condition_variable msgCv;

	std::unordered_map<SOCKET, std::deque<OutMsg>> outgoingMessages;
	std::mutex outgoingMutex;

	const std::string CLIENT_CONNECTION_MESSAGE = "s2uk_connection_init";
	const DWORD CLIENT_TIMEOUT = 30 * 1000;
public:
	bool GetStatus();

	void Connect(int port);

	void Receive(char* outStr, int maxLen = 2048);
	void broadcastMessage(const std::string& msg);

	void CloseSocket();
private:
	void sendQueuedMessages();

	bool sendMessagesToClient(SOCKET clientSock);

	void enqueueMessage(SOCKET sock, const std::string& msg);

	void handleClient(SOCKET clientSock);
};

#endif
