#ifndef CLIENTCONNECTION_H
#define CLIENTCONNECTION_H

#include <cstdint>
#include <string>
#include <thread>
#include <array>
#include <vector>
#include "json.hpp"

#include <WinSock2.h>

using json = nlohmann::json;

constexpr size_t CLIENT_BUFFER_SIZE = 4000;

class ClientConnection
{
    SOCKET socket;
    std::thread thread;
    size_t thread_id;
    bool pendingDelete;
    
    std::array<char, CLIENT_BUFFER_SIZE> recv_buf;
    int bytes_received;
    std::string incomplete_json;
    std::vector<json> jsons_from_client;
    std::vector<json> jsons_to_client;
    
    static void handle_client(ClientConnection* const);
    void setRecvTimeout(int);
    void setSendTimeout(int);
    void onBytesReceived(); // recv > 0
    bool sendAll(std::string const&);

    ClientConnection(const ClientConnection&) = delete;
    ClientConnection& operator=(const ClientConnection&) = delete;
    std::string getClientInfo();
    
    public:
    ClientConnection& operator=(ClientConnection&&);
    ClientConnection(SOCKET&, size_t const);
    bool isPendingToDelete() const;
    void joinThread();
    
    ~ClientConnection();
};

#endif