#pragma once

#include <WinSock2.h>
#include <WS2tcpip.h>

#include <iostream>
#include <sstream>
#include <filesystem>
#include <Windows.h>
#include <mutex>

#include "Session.h"
#include "PacketLittleEndianWriter.h"
#include "PacketHandler.h"
#include "Connector.h"

#define CONNECT_IP_ADDRESS "127.0.0.1"
#define CONNECT_PORT 8923

using namespace std;

mutex response;
void waitResponse();

int parseInt(string);
DWORD WINAPI uiThread(LPVOID lpParamter);
bool isFileExist(string);
string getFileName(string);