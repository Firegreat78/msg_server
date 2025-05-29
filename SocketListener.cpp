#include "SocketListener.h"
#include "Logger.h"
#include "DatabaseConnection.h"

#include <vector>

SocketListener::SocketListener() : next_thread_id(0)
{
    Logger& logger = Logger::getInstance();
    int const startupSuccess = WSAStartup(MAKEWORD(2, 2), &this->wsaData);

    if (startupSuccess != 0) throw std::runtime_error("Failed to initialize WinSock.");

    listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (listenSocket == INVALID_SOCKET) 
    {
        WSACleanup();
        throw std::runtime_error("Failed to create a listening socket.");
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(SERVER_PORT);

    if (bind(listenSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof (serverAddr)) == SOCKET_ERROR)
    {
        closesocket(listenSocket);
        WSACleanup();
        throw std::runtime_error("Failed to bind listening socket.");
    }

    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        closesocket(listenSocket);
        WSACleanup();
        throw std::runtime_error("Failed to begin listening.");
    }

    logger.log(std::string("Server is listening on port ") + std::to_string(SERVER_PORT) + ".");
}

SocketListener::~SocketListener()
{
    closesocket(listenSocket);
    WSACleanup();
}

void SocketListener::listenForConnections()
{
    Logger& logger = Logger::getInstance();

    while (true)
    {
        SOCKET client = accept(listenSocket, nullptr, nullptr);
        std::string const clientStr = std::to_string(client);
        if (client == INVALID_SOCKET)
        {
            std::string const log_msg = std::string("Socket accept failed. Socket: ") + clientStr;
            logger.log(log_msg);
            continue;
        }
        std::string const log_msg = std::string("Socket accept succeded. Socket: ") + clientStr;
        logger.log(log_msg);
        try
        {
            this->connections[next_thread_id] = std::make_unique<ClientConnection>(client, next_thread_id);
            next_thread_id++;
        }
        catch (std::runtime_error const &e)
        {
            std::string const msg = std::string("Failed to create a ClientConnection object: ") + e.what();
            this->connections.erase(next_thread_id);
            logger.log(msg);
            closesocket(client);
        }
        // Now, we need to delete all clients which are disconnected.
        // isPendingToDelete() returns true strictly only if the client thread will be finished, so this is safe.
        std::vector<size_t> to_delete;
        for (auto it = this->connections.begin(); it != this->connections.end(); ++it)
        {
            if (it->second->isPendingToDelete())
            {
                to_delete.push_back(it->first);
                
                // If the user was online at the moment when socket disconnected due to error,
                // we need to check the amount of logins to the same account
                // (two clients may auth to the same account).
                // in case the last client disconnected, set 
                // the 'is_online' value for the user to false. 
                DatabaseConnection::getInstance().onLogonUserDisconnect(it->first);
                
            }
        }
        // We cannot modify the collection while iterating over it
        // (since it would invalidate the iterator), so we need this loop below.
        for (size_t i = 0; i < to_delete.size(); i++)
        {
            // We must join/detach a thread before std::thread object destructor gets called, or
            // otherwise it will call std::terminate.
            // This is designed to prevent undefined behavior caused by resource leaks or dangling references.
            this->connections[to_delete[i]]->joinThread();
            
            // No memory leaked since we are using unique_ptr, and erase() calls destructor of the map value.
            // After we joined the thread, we can safely dispose of the dangling client. 
            this->connections.erase(to_delete[i]);
        }
        to_delete.clear();
    }
}