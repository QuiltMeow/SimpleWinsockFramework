#pragma once

#include <iostream>
#include <WinSock2.h>
#include <mstcpip.h>

#define KEEP_ALIVE_TIME 60
#define KEEP_ALIVE_INTERVAL 5

SOCKET connectServer(SOCKADDR_IN target) {
	SOCKET ret = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (ret == INVALID_SOCKET) {
		std::cerr << "無效的 Socket" << std::endl;
		return INVALID_SOCKET;
	}

	BOOL enable = TRUE;
	if (setsockopt(ret, SOL_SOCKET, SO_KEEPALIVE, (PCHAR)&enable, sizeof(enable)) != 0) {
		std::cerr << "Socket 無法設定 Keep Alive 狀態" << std::endl;
		closesocket(ret);
		return INVALID_SOCKET;
	}
	if (setsockopt(ret, IPPROTO_TCP, TCP_NODELAY, (PCHAR)&enable, sizeof(enable)) != 0) {
		std::cerr << "Socket 無法設定 No Delay 狀態" << std::endl;
		closesocket(ret);
		return INVALID_SOCKET;
	}

	tcp_keepalive keepAlive;
	keepAlive.onoff = 1;
	keepAlive.keepalivetime = KEEP_ALIVE_TIME * 1000;
	keepAlive.keepaliveinterval = KEEP_ALIVE_INTERVAL * 1000;
	DWORD returnByte;
	if (WSAIoctl(ret, SIO_KEEPALIVE_VALS, &keepAlive, sizeof(keepAlive), NULL, 0, &returnByte, NULL, NULL) != 0) {
		std::cerr << "Socket 無法設定 Keep Alive 時間" << std::endl;
		closesocket(ret);
		return INVALID_SOCKET;
	}

	std::cout << "正在連線到伺服器 ..." << std::endl;
	if (connect(ret, (SOCKADDR*)&target, sizeof(target)) != 0) {
		std::cerr << "無法與伺服器建立連線 ..." << std::endl;
		closesocket(ret);
		return INVALID_SOCKET;
	}
	return ret;
}