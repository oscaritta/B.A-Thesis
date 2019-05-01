#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <windows.h>
#include <fstream>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <stdio.h>
#include "Util.h"

#pragma warning(disable:4996)

class Networking
{
private:
	Networking(std::string serverAddr /*local*/, std::string serverAddrRemote, std::string port) 
	{ 
		m_Port = port;  
		m_ServerAddr = serverAddr;  
		m_ServerAddrRemote = serverAddrRemote;
		m_ConnectSocket = INVALID_SOCKET;
		VERBOSE = true;
	}
	Networking() {}
	bool VERBOSE;
	static Networking* m_Instance;
	std::string m_Port, m_ServerAddr, m_ServerAddrRemote;
	SOCKET m_ListenSocket, m_ConnectSocket, m_ClientSocket;

public:
	static int sendUDPPacket(SOCKET s, const char* ip, const char* port, char* buffer, int size)
	{
		sockaddr_in sTargetDevice;
		sTargetDevice.sin_family = AF_INET;
		sTargetDevice.sin_addr.s_addr = inet_addr(ip);
		sTargetDevice.sin_port = htons(atoi(port));
		int sent = sendto(s, buffer, size, 0, (SOCKADDR *)&sTargetDevice, sizeof(SOCKADDR_IN));
		return sent;
	}
	static SOCKET getBoundUDPSocket(int port, int timeout_recv)
	{
		printf("getting udp socket bound to port %d\n", port);
		SOCKET s;
		struct addrinfo *result = NULL, *ptr = NULL, hints;

		ZeroMemory(&hints, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_protocol = IPPROTO_UDP;
		hints.ai_flags = AI_PASSIVE;

		// Resolve the local address and port to be used by the server
		int iResult = getaddrinfo(NULL, std::to_string(port).c_str(), &hints, &result);
		if (iResult != 0) 
		{
			printf("getaddrinfo failed: %d\n", iResult);
			ERROR_MSG("getaddrinfo failed: %d\n");
			WSACleanup();
			return 1;
		}

		s = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
		if (s == INVALID_SOCKET) 
		{
			printf("Error at socket(): %ld\n", WSAGetLastError());
			freeaddrinfo(result);
			WSACleanup();
			return 1;
		}

		// Setup the TCP listening socket
		iResult = bind(s, result->ai_addr, (int)result->ai_addrlen);
		if (iResult == SOCKET_ERROR) 
		{
			ERROR_MSG("Could not bind UDP socket");
			return INVALID_SOCKET;
		}

		freeaddrinfo(result);

		if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout_recv, sizeof(int)))
			return INVALID_SOCKET;

		return s;
	}

	static Networking* getInstance(std::string serverAddr /*local*/, std::string serverAddrRemote, std::string port)
	{
		if (m_Instance == nullptr)
		{
			m_Instance = new Networking(serverAddr, serverAddrRemote, port);
			
			WSADATA wsaData;
			int iResult;

			// Initialize Winsock
			iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
			if (iResult != 0) {
				printf("WSAStartup failed: %d\n", iResult);
				return nullptr;
			}

			return m_Instance;
		}
		return m_Instance;
	}

	int InitializeServer()
	{
		struct addrinfo *result = NULL, *ptr = NULL, hints;
	
		ZeroMemory(&hints, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		hints.ai_flags = AI_PASSIVE;
	
		// Resolve the local address and port to be used by the server
		int iResult = getaddrinfo(NULL, m_Port.c_str(), &hints, &result);
		if (iResult != 0) {
			printf("getaddrinfo failed: %d\n", iResult);
			ERROR_MSG("getaddrinfo failed: %d\n");
			WSACleanup();
			return 1;
		}

		SOCKET ListenSocket = INVALID_SOCKET;
		ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
		if (ListenSocket == INVALID_SOCKET) {
			printf("Error at socket(): %ld\n", WSAGetLastError());
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

		if (listen(ListenSocket, SOMAXCONN) == SOCKET_ERROR) {
			printf("Listen failed with error: %ld\n", WSAGetLastError());
			closesocket(ListenSocket);
			WSACleanup();
			return 1;
		}

		printf("Listening on port %s\n", m_Port.c_str());
		m_ListenSocket = ListenSocket;
		return 0;
	}

public:
	int WaitForClient(const char* partnerIpLocal, const char* partnerIpGlobal, const char* partnerPortLocal, const char* partnerPortGlobal, char** connectedIp)
	{
		PunchPort(partnerIpLocal, partnerPortLocal);
		PunchPort(partnerIpGlobal, partnerPortGlobal);

		std::ofstream signal("readytoserve.txt");
		signal << "yes";
		signal.close();

		SOCKET ClientSocket;
		ClientSocket = INVALID_SOCKET;
		// Accept a client socket
		printf("Listening for incoming connection\n");

		if (InitializeServer())
		{
			ERROR_MSG("657 InitializeServer() returned positive");
			return 1;
		}

		do
		{
			ClientSocket = accept(m_ListenSocket, NULL, NULL);
			if (ClientSocket == INVALID_SOCKET) {
				printf("accept failed: %d\n", WSAGetLastError());
				closesocket(m_ListenSocket);
				WSACleanup();
				return 1;
			}
			sockaddr_in name;
			int size = sizeof(name);
			if (getpeername(ClientSocket, (struct sockaddr*)&name, &size) == 0)
			{
				char *peer_name = inet_ntoa(name.sin_addr);
				USHORT peer_port = ntohs(name.sin_port);

				printf("Peer = %s:%uh | Partner = %s:%s/%s:%s\n", peer_name, peer_port, partnerIpLocal, partnerPortLocal, partnerIpGlobal, partnerPortGlobal);
				if (!strcmp(peer_name, partnerIpLocal) || !strcmp(peer_name, partnerIpGlobal))
				{
					printf("Peer and partner names match...checking ports");
					if (peer_port == atoi(partnerPortLocal) || peer_port == atoi(partnerPortGlobal))
					{
						printf("Peer and partner ports match\n");
						printf("Connection established\n");
						m_ClientSocket = ClientSocket;
						closesocket(m_ListenSocket);
						*connectedIp = peer_name;

						std::ofstream signal("session_started.txt");
						signal << "yes";
						signal.close();

						break;
					}
					else
					{
						printf("Peer and partner ports don't match...closing this connection");
						closesocket(ClientSocket);
						continue;
					}
				}
				else
				{
					printf("Peer and partner don't match...closing this connection");
					closesocket(ClientSocket);
					continue;
				}
			}
			else
			{
				printf("Could not get peer name: %i\n", WSAGetLastError());
				closesocket(ClientSocket);
				continue;
			}


		} while (true);
		return 0;
	}

	int ConnectToServer(const char* ip, const char* port)
	{
		int iResult;

		struct addrinfo *result = NULL, *ptr = NULL, hints;

		ZeroMemory(&hints, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;

		printf("Connecting to %s:%s\n", ip, port);

		// Resolve the server address and port
		iResult = getaddrinfo(ip, port, &hints, &result);
		if (iResult != 0) {
			printf("getaddrinfo failed with error: %d\n", iResult);
			ERROR_MSG("getaddrinfo failed: %d\n");
			WSACleanup();
			return 1;
		}

		m_ConnectSocket = GetBoundSocket();

		// Attempt to connect to an address until one succeeds
		for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
			// Connect to server.
			iResult = connect(m_ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
			if (iResult == SOCKET_ERROR) {
				closesocket(m_ConnectSocket);
				m_ConnectSocket = INVALID_SOCKET;
				continue;
			}
			break;
		}

		freeaddrinfo(result);

		if (m_ConnectSocket == INVALID_SOCKET) {
			printf("Unable to connect to server!\n");
			return 1;
		}

		printf("Connected to server!\n");
		return 0;
	}

	int SendToClient(char *buffer, int len)
	{
		int iSendResult = send(m_ClientSocket, (char*)&len, sizeof(int), 0);
		if (iSendResult == SOCKET_ERROR)
		{
			printf("send failed: %d\n", WSAGetLastError());
			closesocket(m_ClientSocket);
			WSACleanup();
		}
		else
		{
			iSendResult = send(m_ClientSocket, buffer, len, 0);
			if (iSendResult == SOCKET_ERROR)
			{
				printf("send failed: %d\n", WSAGetLastError());
				closesocket(m_ClientSocket);
				WSACleanup();
			}
		}
		return iSendResult;
	}

	int SendToServer(const char *buffer, int len)
	{
		int iSendResult = send(m_ConnectSocket, (char*)&len, sizeof(int), 0);
		if (iSendResult == SOCKET_ERROR)
		{

			printf("send failed: %d\n", WSAGetLastError());

			closesocket(m_ConnectSocket);
			WSACleanup();
		}
		else
		{
			iSendResult = send(m_ConnectSocket, buffer, len, 0);
			if (iSendResult == SOCKET_ERROR)
			{

				printf("send failed: %d\n", WSAGetLastError());

				closesocket(m_ConnectSocket);
				WSACleanup();
			}
		}
		return iSendResult;
	}

	int ReceiveFromClient(char *buffer, int maxlen)
	{
		int iResult;
		int to_read;

		iResult = recv(m_ClientSocket, (char*)&to_read, sizeof(int), 0);

		if (VERBOSE) printf("+++We need to read from client %d\n", to_read);//printf("+++We need to read from client %d\n", to_read);


		if (iResult == 0)
		{

			printf("Connection closing...\n");

			return iResult;
		}
		if (to_read < 0)
		{
			MessageBox(nullptr, (std::string("Error: to_read(") + std::to_string(to_read) + std::string(") is negatiive")).c_str(), "Error", MB_OK | MB_ICONERROR);
			while (true);
		}
		else if (iResult == SOCKET_ERROR)
		{

			printf("recv failed: %d\n", WSAGetLastError());

			closesocket(m_ClientSocket);
			WSACleanup();
			return iResult;
		}

		if (iResult != sizeof(int))
		{
			MessageBox(nullptr, "Fatal error: iResult != sizeof(int) when receiving from client. Exiting", "Error", MB_OK | MB_ICONERROR);
			ExitProcess(0);
		}

		iResult = recv(m_ClientSocket, buffer, to_read, 0);
		if (iResult == 0) 
		{

			printf("Connection closing...\n");

		}
		else if (iResult == SOCKET_ERROR)
		{

			printf("recv failed: %d\n", WSAGetLastError());

			closesocket(m_ClientSocket);
			WSACleanup();
		}
		else if (iResult != to_read)
		{
			//MessageBox(nullptr, "Fatal error: iResult != to_read when receiving from client. Exiting", "Error", MB_OK);
			//ExitProcess(0);
			while (iResult < to_read)
			{

				if (VERBOSE) printf("Incomplete read: we still have bytes to read from client\n ", to_read - iResult);

				iResult += recv(m_ConnectSocket, buffer + iResult, to_read - iResult, 0);
			}

			if (VERBOSE) printf("After correction, we've read from the client %d bytes\n" , iResult);

			if (iResult > to_read)
			{
				MessageBox(nullptr, "But still...now iResult > to_read", "Error", MB_OK);
				printf("iResult = %d > to_read = %d\n", iResult , to_read);
				//while (true);
			}
		}

		return iResult;
	}

	int ReceiveFromServer(char *buffer, int maxlen)
	{
		int iResult;
		int to_read;

		iResult = recv(m_ConnectSocket, (char*)&to_read, sizeof(int), 0);

		if (VERBOSE) printf("+++We need to read from server %d\n ", to_read);

		if (to_read < 0)
		{
			MessageBox(nullptr, (std::string("Error: to_read(") + std::to_string(to_read) + std::string(") is negative")).c_str(), "Error", MB_OK | MB_ICONERROR);
			while (true);
		}
		if (iResult == 0)
		{
			printf("Connection closing...\n");
			return iResult;
		}
		else if (iResult == SOCKET_ERROR)
		{
			printf("recv failed: %d\n", WSAGetLastError());
			closesocket(m_ConnectSocket);
			WSACleanup();
			return iResult;
		}

		if (iResult != sizeof(int))
		{
			MessageBox(nullptr, "Fatal error: iResult != sizeof(int) when sending to server. Exiting", "Error", MB_OK | MB_ICONERROR);
			ExitProcess(0);
		}

		iResult = recv(m_ConnectSocket, buffer, to_read, 0);
		if (iResult == 0)
			printf("Connection closing...\n");
		else if (iResult == SOCKET_ERROR)
		{
			printf("recv failed: %d\n", WSAGetLastError());
			closesocket(m_ConnectSocket);
			WSACleanup();
		}
		else if (iResult != to_read)
		{
			while (iResult < to_read)
			{

				if (VERBOSE) printf("Incomplete read: we still have %d bytes to read from the server\n", to_read - iResult);

				iResult  += recv(m_ConnectSocket, buffer+iResult, to_read-iResult, 0);
			}

			if (VERBOSE) printf("After correction, we've read from the server %d bytes\n", iResult);

			//MessageBox(nullptr, "Fatal error: iResult != to_read when receiving from client. Exiting", "Error", MB_OK);
			//ExitProcess(0);
			if (iResult > to_read)
			{
				MessageBox(nullptr, "But still...now iResult > to_read", "Error", MB_OK);
				printf("iResult = %d > to_read = %d\n" , iResult , to_read);
				//while (true);
			}
		}

		return iResult;
	}

private:
	SOCKET GetBoundSocket()
	{
		struct addrinfo *result = NULL, *ptr = NULL, hints;
		ZeroMemory(&hints, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		hints.ai_flags = AI_PASSIVE;
		// Resolve the local address and port to be used by the server
		int iResult = getaddrinfo(NULL, m_Port.c_str(), &hints, &result);
		if (iResult != 0) {
			printf("getaddrinfo failed: %d\n", iResult);
			ERROR_MSG("getaddrinfo failed: %d\n");
			WSACleanup();
			return 1;
		}
		SOCKET ListenSocket = INVALID_SOCKET;
		ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
		if (ListenSocket == INVALID_SOCKET) {
			printf("Error at socket(): %ld\n", WSAGetLastError());
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
		return ListenSocket;
	}

	int PunchPort(const char* partnerIp, const char* partnerPort)
	{
		struct addrinfo *result = NULL, *ptr = NULL, hints;
		int iResult;
		SOCKET outgoingSocket = GetBoundSocket();

		printf("Punching hole for %s:%s\n", partnerIp, partnerPort);

		// Resolve the server address and port
		ZeroMemory(&hints, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;

		// Resolve the server address and port
		iResult = getaddrinfo(partnerIp, partnerPort, &hints, &result);
		if (iResult != 0) {
			printf("getaddrinfo failed with error: %d\n", iResult);
			ERROR_MSG("getaddrinfo failed: %d\n");
			WSACleanup();
			return 1;
		}
		// Attempt to connect to an address until one succeeds

		for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
			// Connect to server.
			iResult = connect(outgoingSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
			if (iResult == SOCKET_ERROR) {
				continue;
			}
			printf("Partner connection established\n");
			break;
		}
		freeaddrinfo(result);

		printf("Hole punched\n");

		closesocket(outgoingSocket);
	}
	
};

Networking* Networking::m_Instance = nullptr;