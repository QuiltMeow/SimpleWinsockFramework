#pragma once

#include <iostream>
#include <WinSock2.h>
#include <mstcpip.h>

#define KEEP_ALIVE_TIME 60
#define KEEP_ALIVE_INTERVAL 5

SOCKET connectServer(SOCKADDR_IN target) {
	SOCKET ret = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (ret == INVALID_SOCKET) {
		std::cerr << "�L�Ī� Socket" << std::endl;
		return INVALID_SOCKET;
	}

	BOOL enable = TRUE;
	if (setsockopt(ret, SOL_SOCKET, SO_KEEPALIVE, (PCHAR)&enable, sizeof(enable)) != 0) {
		std::cerr << "Socket �L�k�]�w Keep Alive ���A" << std::endl;
		closesocket(ret);
		return INVALID_SOCKET;
	}
	if (setsockopt(ret, IPPROTO_TCP, TCP_NODELAY, (PCHAR)&enable, sizeof(enable)) != 0) {
		std::cerr << "Socket �L�k�]�w No Delay ���A" << std::endl;
		closesocket(ret);
		return INVALID_SOCKET;
	}

	tcp_keepalive keepAlive;
	keepAlive.onoff = 1;
	keepAlive.keepalivetime = KEEP_ALIVE_TIME * 1000;
	keepAlive.keepaliveinterval = KEEP_ALIVE_INTERVAL * 1000;
	DWORD returnByte;
	if (WSAIoctl(ret, SIO_KEEPALIVE_VALS, &keepAlive, sizeof(keepAlive), NULL, 0, &returnByte, NULL, NULL) != 0) {
		std::cerr << "Socket �L�k�]�w Keep Alive �ɶ�" << std::endl;
		closesocket(ret);
		return INVALID_SOCKET;
	}

	std::cout << "���b�s�u����A�� ..." << std::endl;
	if (connect(ret, (SOCKADDR*)&target, sizeof(target)) != 0) {
		std::cerr << "�L�k�P���A���إ߳s�u ..." << std::endl;
		closesocket(ret);
		return INVALID_SOCKET;
	}
	return ret;
}