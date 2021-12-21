#pragma once

#define FD_SETSIZE 5000
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <mstcpip.h>

#include <conio.h>
#include <iostream>
#include <map>
#include <mutex>
#include <Windows.h>

#include "PacketLittleEndianReader.h"
#include "PacketLittleEndianWriter.h"
#include "Session.h"

#define PORT 8923
#define MAX_PENDING_CONNECTION 1000
#define BUFFER_SIZE 4096

// Keep Alive 時間 (單位 : 秒)
#define KEEP_ALIVE_TIME 60
#define KEEP_ALIVE_INTERVAL 5
#define IDLE_WAIT_TIME 1

using namespace std;

HANDLE acceptThreadHandle;
HANDLE receiveThreadHandle;

volatile atomic<bool> run = true;
int clientCount = 0;
map<SOCKET, Session*> sessionMap;
mutex socketMutex;

SOCKET server;
fd_set fdClientSocket;

bool (*onClientConnect)(SOCKET, SOCKADDR_IN) = NULL;
void (*onPacketReceive)(Session&, byte*, int);
void (*onClientDisconnect)(SOCKET) = NULL;
void (*onUserServerClose)() = NULL;

DWORD WINAPI acceptThread(LPVOID);
DWORD WINAPI receiveThread(LPVOID);
void closeServer();
void removeClient(SOCKET);
SOCKET initServer();