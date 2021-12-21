#pragma once

#include <iostream>
#include <mutex>
#include <fstream>
#include <condition_variable>
#include <WinSock2.h>
#include <Windows.h>

#include "Session.h"
#include "PacketLittleEndianReader.h"
#include "PacketLittleEndianWriter.h"

#define BUFFER_SIZE 4096

SOCKET client = INVALID_SOCKET;
Session* session;
HANDLE applicationThreadHandle;

bool hasNotify = false;
std::condition_variable responseCondition;
std::mutex closeMutex;

void disconnect() {
	std::lock_guard<std::mutex> lock(closeMutex);
	if (client != INVALID_SOCKET) {
		closesocket(client);
		client = INVALID_SOCKET;
	}
}

void stopApplicationThread() {
	disconnect();
#pragma warning(suppress : 6258)
	TerminateThread(applicationThreadHandle, EXIT_SUCCESS);
}

DWORD WINAPI receiveThread(LPVOID lpParameter) {
	char buffer[BUFFER_SIZE];
	while (true) {
		int receiveLength = recv(client, buffer, sizeof(buffer), 0);
		if (receiveLength <= 0) {
			stopApplicationThread();
			system("cls");
			system("echo 中斷連線 ...");
			break;
		}
		session->receive(buffer, receiveLength);
		if (session->isDisconnect()) {
			disconnect();
		}
	}
	return EXIT_SUCCESS;
}

void sendPacket(Session* target, PacketLittleEndianWriter& writer) {
	BYTE* packet = writer.getPacket();
	target->sendPacket(packet, writer.size());
	delete[] packet;
}

enum PacketOPCode {
	GET_INFO = 0x00,
	GET_AVATAR = 0x01,
	FILE_UPLOAD = 0x02,
	GET_RANDOM_NUMBER = 0x03,
	ECHO = 0x04,
	DISCONNECT = 0x05
};

void handlePacket(Session& session, BYTE* data, int length) {
	PacketLittleEndianReader packet(data, length);
	delete[] data;

	try {
		short op = packet.readShort();
		switch (op) {
		case GET_INFO: {
			std::cout << "===== 服務端回應資訊 =====" << std::endl;
			std::cout << packet.readLengthAsciiString() << std::endl;
			break;
		}
		case GET_AVATAR: {
			std::string fileName = packet.readLengthAsciiString();
			replace(fileName.begin(), fileName.end(), '\\', '/');
			fileName.erase(remove(fileName.begin(), fileName.end(), '/'), fileName.end());

			std::pair<BYTE*, int> file = packet.readFile();
			if (file.second < 0) {
				std::cerr << "接收到無效檔案" << std::endl;
				break;
			}

			std::fstream fs(fileName, std::ios::out | std::ios::binary);
			if (!fs) {
				std::cerr << "圖標取得失敗" << std::endl;
				delete[] file.first;
				break;
			}
			if (file.second > 0) {
				fs.write((const char*)file.first, file.second);
			}
			fs.close();
			delete[] file.first;
			std::cout << "圖標下載完成 檔案名稱 : " << fileName << std::endl;
			break;
		}
		case GET_RANDOM_NUMBER: {
			std::cout << "服務端回應數值 : " << packet.readInt() << std::endl;
			break;
		}
		case ECHO: {
			std::cout << "===== 服務端回應訊息 =====" << std::endl;
			std::cout << packet.readLengthAsciiString() << std::endl;
			break;
		}
		}
	}
	catch (std::exception& ex) {
		std::cerr << "解析封包時發生例外狀況 : " << ex.what() << std::endl;
	}
	hasNotify = true;
	responseCondition.notify_all();
}