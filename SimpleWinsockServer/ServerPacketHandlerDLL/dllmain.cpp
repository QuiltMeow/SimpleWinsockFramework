#include "pch.h"
#include "PacketLittleEndianReader.h"
#include "PacketLittleEndianWriter.h"
#include "SessionLib.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include "Session.h"
#include <random>

using namespace std;

void packetHandler(Session&, BYTE*, int);

extern "C" {
	__declspec(dllexport) void* setupPacketReceive();
}

enum PacketOPCode {
	GET_INFO = 0x00,
	GET_AVATAR = 0x01,
	FILE_UPLOAD = 0x02,
	GET_RANDOM_NUMBER = 0x03,
	ECHO = 0x04
};

random_device randomDevice;
default_random_engine randomEngine(randomDevice());
uniform_int_distribution<int> randomDistribution(0, RAND_MAX);

void packetHandler(Session& session, BYTE* data, int length) {
	PacketLittleEndianReader packet(data, length);
	delete[] data;

	try {
		short op = packet.readShort();

		PacketLittleEndianWriter writer;
		writer.writeShort(op);
		switch (op) {
		case GET_INFO: {
			writer.writeLengthAsciiString("Simple Quilt Winsock Server\r\n這是一個演示用的服務端\r\n貓咪最可愛了唷 喵嗚 <3 ~");
			sendPacket(session, writer);
			break;
		}
		case GET_AVATAR: {
			const string FILE_NAME = "Avatar.png";
			writer.writeLengthAsciiString(FILE_NAME);
			writer.writeFile(FILE_NAME);
			sendPacket(session, writer);
			break;
		}
		case FILE_UPLOAD: {
			string ip = session.getReadableIPAddress();
			replace(ip.begin(), ip.end(), ':', '_');

			string directory = "download/" + ip;
			string fileName = packet.readLengthAsciiString();
			replace(fileName.begin(), fileName.end(), '\\', '/');
			fileName.erase(remove(fileName.begin(), fileName.end(), '/'), fileName.end());
			string filePath = directory + "/" + fileName;

			if (filesystem::is_directory(directory) || filesystem::create_directories(directory)) {
				pair<BYTE*, int> file = packet.readFile();
				if (file.first == NULL) {
					break;
				}
				fstream fs(filePath, ios::out | ios::binary);
				if (fs) {
					cout << "接收檔案 : " << filePath << endl;
					if (file.second > 0) {
						fs.write((const char*)file.first, file.second);
					}
				}
				fs.close();
				delete[] file.first;
			}
			break;
		}
		case GET_RANDOM_NUMBER: {
			writer.writeInt(randomDistribution(randomEngine));
			sendPacket(session, writer);
			break;
		}
		case ECHO: {
			writer.writeLengthAsciiString(packet.readLengthAsciiString());
			sendPacket(session, writer);
			break;
		}
		}
	}
	catch (exception& ex) {
		cerr << "解析封包時發生例外狀況 : " << ex.what() << endl;
	}
}

__declspec(dllexport) void* setupPacketReceive() {
	return packetHandler;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
	switch (ul_reason_for_call) {
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH: {
		break;
	}
	}
	return TRUE;
}