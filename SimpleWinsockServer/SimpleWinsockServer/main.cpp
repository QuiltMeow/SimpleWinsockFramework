#include "main.h"

int main() {
	FD_ZERO(&fdClientSocket);

	cout << "初始化封包處理器 ..." << endl;
	HINSTANCE library = LoadLibraryA("PacketHandler.dll");
	if (library == NULL) {
		cerr << "封包處理器載入失敗" << endl;
		return EXIT_FAILURE;
	}
	void* (*setupPacketReceive)() = (void* (*)()) GetProcAddress(library, "setupPacketReceive");
	if (setupPacketReceive == NULL) {
		cerr << "無法取得封包處理器載入函式" << endl;
		return EXIT_FAILURE;
	}
	onPacketReceive = (void(*)(Session&, byte*, int)) setupPacketReceive();
	if (onPacketReceive == NULL) {
		cerr << "封包處理器載入函式實作錯誤" << endl;
		return EXIT_FAILURE;
	}

	void* (*setupClientConnect)() = (void* (*)()) GetProcAddress(library, "setupClientConnect");
	if (setupClientConnect != NULL) {
		cout << "註冊客戶端連線事件" << endl;
		onClientConnect = (bool(*)(SOCKET, SOCKADDR_IN)) setupClientConnect();
	}

	void* (*setupClientDisconnect)() = (void* (*)()) GetProcAddress(library, "setupClientDisconnect");
	if (setupClientDisconnect != NULL) {
		cout << "註冊客戶端斷線事件" << endl;
		onClientDisconnect = (void(*)(SOCKET)) setupClientDisconnect();
	}

	void* (*setupUserServerClose)() = (void* (*)()) GetProcAddress(library, "setupUserServerClose");
	if (setupUserServerClose != NULL) {
		cout << "註冊使用者關閉服務端事件" << endl;
		onUserServerClose = (void(*)()) setupUserServerClose();
	}

	WSADATA wsaData;
	WORD version = MAKEWORD(2, 2);
	if (WSAStartup(version, &wsaData) != 0) {
		cerr << "WSA 初始化失敗" << endl;
		return EXIT_FAILURE;
	}

	SOCKET init = initServer();
	if (init == INVALID_SOCKET) {
		WSACleanup();
		return EXIT_FAILURE;
	}
	server = init;

	cout << "建立收包執行緒 ..." << endl;
	receiveThreadHandle = CreateThread(NULL, 0, receiveThread, NULL, 0, NULL);
	if (receiveThreadHandle == NULL) {
		cerr << "收包執行緒建立失敗" << endl;
		closesocket(server);
		WSACleanup();
		return EXIT_FAILURE;
	}

	cout << "建立接受連線執行緒 ..." << endl;
	acceptThreadHandle = CreateThread(NULL, 0, acceptThread, NULL, 0, NULL);
	if (acceptThreadHandle == NULL) {
		cerr << "接受連線執行緒建立失敗" << endl;
		closeServer();
		WSACleanup();
		return EXIT_FAILURE;
	}

	cout << "按下 ESC 按鍵結束程式" << endl;
	while (_getch() != VK_ESCAPE);

	cout << "關閉服務端 ..." << endl;
	if (onUserServerClose != NULL) {
		onUserServerClose();
	}
	closeServer();
	WSACleanup();
	return EXIT_SUCCESS;
}

void closeServer() {
	run = false;
	closesocket(server);

	WaitForSingleObject(receiveThreadHandle, INFINITE);
	CloseHandle(receiveThreadHandle);
	if (acceptThreadHandle != NULL) {
		WaitForSingleObject(acceptThreadHandle, INFINITE);
		CloseHandle(acceptThreadHandle);
	}

	for (int i = 0; i < fdClientSocket.fd_count; ++i) {
		closesocket(fdClientSocket.fd_array[i]);
	}
	FD_ZERO(&fdClientSocket);
	for (auto const& sessionEntry : sessionMap) {
		delete sessionEntry.second;
	}
	sessionMap.clear();
	clientCount = 0;
}

SOCKET initServer() {
	SOCKET ret = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (ret == INVALID_SOCKET) {
		cerr << "無效的 Socket" << endl;
		return INVALID_SOCKET;
	}

	BOOL enable = TRUE;
	if (setsockopt(ret, SOL_SOCKET, SO_KEEPALIVE, (PCHAR)&enable, sizeof(enable)) != 0) {
		cerr << "Socket 無法設定 Keep Alive 狀態" << endl;
		closesocket(ret);
		return INVALID_SOCKET;
	}
	if (setsockopt(ret, IPPROTO_TCP, TCP_NODELAY, (PCHAR)&enable, sizeof(enable)) != 0) {
		cerr << "Socket 無法設定 No Delay 狀態" << endl;
		closesocket(ret);
		return INVALID_SOCKET;
	}

	tcp_keepalive keepAlive;
	keepAlive.onoff = 1;
	keepAlive.keepalivetime = KEEP_ALIVE_TIME * 1000;
	keepAlive.keepaliveinterval = KEEP_ALIVE_INTERVAL * 1000;
	DWORD returnByte;
	if (WSAIoctl(ret, SIO_KEEPALIVE_VALS, &keepAlive, sizeof(keepAlive), NULL, 0, &returnByte, NULL, NULL) != 0) {
		cerr << "Socket 無法設定 Keep Alive 時間" << endl;
		closesocket(ret);
		return INVALID_SOCKET;
	}

	SOCKADDR_IN local;
	ZeroMemory(&local, sizeof(local));
	local.sin_addr.s_addr = INADDR_ANY;
	local.sin_family = AF_INET;
	local.sin_port = htons(PORT);
	if (bind(ret, (SOCKADDR*)&local, sizeof(local)) != 0) {
		cerr << "服務端端口綁定失敗" << endl;
		closesocket(ret);
		return INVALID_SOCKET;
	}

	if (listen(ret, MAX_PENDING_CONNECTION) != 0) {
		cerr << "服務端端口監聽失敗" << endl;
		closesocket(ret);
		return INVALID_SOCKET;
	}
	return ret;
}

DWORD WINAPI acceptThread(LPVOID lpParameter) {
	SOCKADDR_IN clientAddress;
	int clientAddressLength = sizeof(clientAddress);

	cout << "服務端就緒 準備接受連線" << endl;
	while (run) {
		SOCKET client = accept(server, (SOCKADDR*)&clientAddress, &clientAddressLength);
		if (client == INVALID_SOCKET) {
			continue;
		}
		if (clientCount >= FD_SETSIZE) {
			closesocket(client);
			continue;
		}

		u_long nonBlock = TRUE;
		if (ioctlsocket(client, FIONBIO, &nonBlock) != 0) {
			closesocket(client);
			continue;
		}

		socketMutex.lock();
		if (onClientConnect != NULL) {
			if (!onClientConnect(client, clientAddress)) {
				socketMutex.unlock();
				closesocket(client);
				continue;
			}
		}

		char ipString[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &(clientAddress.sin_addr), ipString, INET_ADDRSTRLEN);
		cout << "[接受連線] IP : " << ipString << ":" << clientAddress.sin_port << " 已連線到您的伺服器" << endl;
		FD_SET(client, &fdClientSocket);
		sessionMap[client] = new Session(client, clientAddress, onPacketReceive);
		++clientCount;
		socketMutex.unlock();
	}
	return EXIT_SUCCESS;
}

void removeClient(SOCKET client) {
	lock_guard<mutex> lock(socketMutex);
	closesocket(client);
	FD_CLR(client, &fdClientSocket);
	delete sessionMap[client];
	sessionMap.erase(client);
	--clientCount;

	if (onClientDisconnect != NULL) {
		onClientDisconnect(client);
	}
}

#pragma warning(suppress : 6262)
DWORD WINAPI receiveThread(LPVOID lpParameter) {
	char buffer[BUFFER_SIZE];
	timeval timeout; // 選擇模型 : 最多等候 1 秒
	timeout.tv_sec = IDLE_WAIT_TIME;
	timeout.tv_usec = 0;

	while (run) {
		socketMutex.lock();
		fd_set fdRead = fdClientSocket;
		socketMutex.unlock();

		if (fdRead.fd_count <= 0) {
			Sleep(IDLE_WAIT_TIME * 1000);
			continue;
		}

		int selectCount = select(0, &fdRead, NULL, NULL, &timeout);
		if (selectCount <= 0) {
			continue;
		}

		for (int i = 0; i < fdRead.fd_count; ++i) {
			SOCKET client = fdRead.fd_array[i];
			int receiveLength = recv(client, buffer, sizeof(buffer), 0);
			if (receiveLength <= 0) {
				removeClient(client);
				continue;
			}

			Session* session = sessionMap[client];
			session->receive(buffer, receiveLength);
			if (session->isDisconnect()) {
				removeClient(client);
			}
		}
	}
	return EXIT_SUCCESS;
}