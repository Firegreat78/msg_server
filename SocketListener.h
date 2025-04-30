#ifndef SOCKETLISTENER_H
#define SOCKETLISTENER_H

#include <cstdint>
#include <map>

#include "ClientConnection.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>

class SocketListener
{
    static constexpr uint16_t SERVER_PORT = 6000;
    uint64_t next_thread_id;
    std::map<size_t, std::unique_ptr<ClientConnection>> connections;
    WSADATA wsaData;
    SOCKET listenSocket;
    sockaddr_in serverAddr;

    public:
    SocketListener();
    void listenForConnections();
    ~SocketListener();
};

#endif