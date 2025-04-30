#include "ClientConnection.h"
#include "Logger.h"

#include "json.hpp"
using json = nlohmann::json;

#include <string>
#include <vector>

ClientConnection::ClientConnection(SOCKET& socket, size_t const thread_id)
: socket(socket), thread_id(thread_id), pendingDelete(false), incomplete_json("")
{
    setRecvTimeout(10000);
    setSendTimeout(10000);
    this->thread = std::thread(&ClientConnection::handle_client, this);
}

ClientConnection::~ClientConnection()
{
    closesocket(socket);
    std::string const msg = std::string("Client with thread id ") + 
    this->getClientInfo() + "has disconnected from the server.";
    Logger::getInstance().log(msg);
}

// For each json we received from the client, we generate a 
// corresponding json which will be sent to the client.
json generateResponse(json const& js)
{
    json answer;
    std::string const type = js["type"].get<std::string>();
    answer["type"] = type + "Response";

    if (type == "userLogin")
    {
        std::string const response_str = 
        std::string("Msg from server: Login successful! Login = ") + 
        js["login"].get<std::string>() + " " + 
        "Password = " + js["password"].get<std::string>();
        answer["type"] = "userLoginAnswer";
        answer["response"] = response_str;
    }
    return answer;
}

bool ClientConnection::sendAll(std::string const& data)
{
    Logger& logger = Logger::getInstance();
    logger.log(std::string("Preparing to send data: ") + data);
    size_t total_sent = 0;

    while (total_sent < data.length())
    {
        int const bytes_sent = send(this->socket, data.data() + total_sent, data.length() - total_sent, 0);
        if (bytes_sent == SOCKET_ERROR)
        {
            std::string msg = std::string("Socket error on send(): ") + std::to_string(WSAGetLastError());
            logger.log(msg);
            return false;
        }
        total_sent += bytes_sent;
    }
    return true;
}

void ClientConnection::handle_client(ClientConnection* const conn)
{
    Logger& logger = Logger::getInstance();
        
    // TODO: before all other actions, send two bytes 0x0001 to determine client machine's endianness.
    
    while (!conn->pendingDelete)
    {
        conn->bytes_received = recv(conn->socket, conn->recv_buf.data(), sizeof (conn->recv_buf) - 1, 0);
        if (conn->bytes_received > 0) conn->onBytesReceived();
        else if (conn->bytes_received == 0) // graceful socket shutdown
        {
            std::string const msg = std::string("Client " ) +
            conn->getClientInfo() + 
            " has disconnected (recv zero bytes).";
            logger.log(msg);
            conn->pendingDelete = true;
        }

        else if (WSAGetLastError() == WSAETIMEDOUT) // recv timeout (no heartbeat from the client...)
        {
            std::string const msg = std::string("Heartbeat timed out from client ") + 
            conn->getClientInfo();
            logger.log(msg);
            conn->pendingDelete = true;
        }
        else // other socket error
        {
            std::string const msg = std::string("Socket error occured for client ") + 
            conn->getClientInfo() + ": " + std::to_string(WSAGetLastError());
            logger.log(msg);
            conn->pendingDelete = true;
        }
        
        for (auto const& js_from_client : conn->jsons_from_client)
        {
            json const js_to_client = generateResponse(js_from_client);
            conn->jsons_to_client.push_back(js_to_client);
        }
        
        for (auto const& js_to_client : conn->jsons_to_client)
        {
            std::string const data = js_to_client.dump();
            bool const send_success = conn->sendAll(data);
            conn->pendingDelete = !send_success;
        }

        conn->jsons_from_client.clear();
        conn->jsons_to_client.clear();
    }
}

// recv() may give an incomplete json.
// For example:
// {"type":"heartbeat","value":52}{"type":"heartbeat","val
// The first JSON is complete, the second one is incomplete.
// To determine the completeness of an json, we use bracket-depth method.
size_t getFirstCompletedIndex(std::string const& chunk)
{
    int depth = 0;
    bool insideString = false;

    for (size_t i = 0; i < chunk.size(); i++)
    {
        char const c = chunk[i];

        if (c == '"')
        {
            size_t bs = 0; // backslashes
            size_t k = i;
            while (k > 0 && chunk[k-1] == '\\')
            {
                ++bs;
                --k;
            }
            if ((bs % 2) == 0) insideString = !insideString;
        }
        
        if (insideString) continue;

        if (c == '{') ++depth;
        if (c == '}') --depth;

        if (depth == 0 && c == '}') return i + 1;
    }
    return 0;
}

void ClientConnection::onBytesReceived()
{
    recv_buf[bytes_received] = 0;
    std::string chunk = incomplete_json + std::string(recv_buf.data());
    while (true)
    {
        size_t const index = getFirstCompletedIndex(chunk);
        if (index == 0) break;
        std::string const json_first = chunk.substr(0, index);
        jsons_from_client.push_back(json::parse(json_first));
        chunk = chunk.substr(index);
    }
    incomplete_json += chunk;
}

std::string ClientConnection::getClientInfo()
{
    return std::string("(socket=") + 
    std::to_string(this->socket) + 
    "; thread_id=" + 
    std::to_string(this->thread_id) + 
    ")";
}

void ClientConnection::setRecvTimeout(int millis)
{
    Logger& logger = Logger::getInstance();
    if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&millis), sizeof(millis)) == SOCKET_ERROR)
    {
        auto msg = std::string("Failed to set socket option for data receive timeout. Socket: ") + std::to_string(socket);
        logger.log(msg);
        throw std::runtime_error(msg);
    }
    auto msg = std::string("Set socket option for data receive timeout successfully. Socket: ") + std::to_string(socket);
}

void ClientConnection::setSendTimeout(int millis)
{
    Logger& logger = Logger::getInstance();
    if (setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&millis), sizeof(millis)) == SOCKET_ERROR)
    {
        auto msg = std::string("Failed to set socket option for data send timeout. Socket: ") + std::to_string(socket);
        logger.log(msg);
        throw std::runtime_error(msg);
    }
    auto msg = std::string("Set socket option for data send timeout successfully. Socket: ") + std::to_string(socket);
}

bool ClientConnection::isPendingToDelete() const
{
    return this->pendingDelete;
}

void ClientConnection::joinThread()
{
    if (this->thread.joinable()) this->thread.join();
}