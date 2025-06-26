#pragma once

#include <string>
#include <stdexcept>

#ifdef _WIN32

#include <WinSock2.h>
#include <ws2tcpip.h>
using socket_t = SOCKET;
using ssize_t = SSIZE_T;
inline int closeSocket(socket_t socket) { return closesocket(socket); }
#pragma comment(lib, "Ws2_32.lib")

#else // POSIX

#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#define INVALID_SOCKET (-1)
using socket_t = int;
inline int closeSocket(socket_t socket) { return close(socket); }

#endif

class Socket {
public:
    // Default constructor
    Socket() : m_socket(INVALID_SOCKET) {}

    // Constructor to create a new socket
    explicit Socket(int family, int type, int protocol) 
    {
        create(family, type, protocol);
    }

    // Constructor for accepted sockets
    explicit Socket(socket_t sock) : m_socket(sock) 
    {
        if (m_socket == INVALID_SOCKET) {
            throw std::runtime_error("Invalid socket descriptor");
        }
    }

    // Destructor
    ~Socket() 
    {
        if (m_socket != INVALID_SOCKET) 
        {
            closeSocket(m_socket);
        }
    }

    // Delete copy operations
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    // Move constructor
    Socket(Socket&& other) noexcept : m_socket(other.m_socket) 
    {
        other.m_socket = INVALID_SOCKET;
    }

    socket_t getSocket() const { return m_socket; }

    // Move assignment
    Socket& operator=(Socket&& other) noexcept 
    {
        if (this != &other) 
        {
            if (m_socket != INVALID_SOCKET) 
            {
                closeSocket(m_socket);
            }
            m_socket = other.m_socket;
            other.m_socket = INVALID_SOCKET;
        }
        return *this;
    }

    // Create a socket
    void create(int family, int type, int protocol) 
    {
        m_socket = socket(family, type, protocol);
        if (m_socket == INVALID_SOCKET) 
        {
            throwError("socket creation");
        }
    }

    // Bind the socket to an address and port
    void bind(const std::string& address, int port) 
    {
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        if (inet_pton(AF_INET, address.c_str(), &addr.sin_addr) <= 0) 
        {
            throw std::runtime_error("Invalid address");
        }
        int result = ::bind(m_socket, (sockaddr*)&addr, sizeof(addr));
        checkError(result, "bind");
    }

    // Listen for incoming connections
    void listen(int backlog) 
    {
        int result = ::listen(m_socket, backlog);
        checkError(result, "listen");
    }

    // Accept an incoming connection
    Socket accept() 
    {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        socket_t client_socket = ::accept(m_socket, (sockaddr*)&client_addr, &client_len);

        if (client_socket == INVALID_SOCKET) 
        {
            throwError("accept");
        }

        return Socket(client_socket);
    }

    // Connect to a remote server
    void connect(const std::string& address, int port) 
    {
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        if (inet_pton(AF_INET, address.c_str(), &addr.sin_addr) <= 0) 
        {
            throw std::runtime_error("Invalid address");
        }
        int result = ::connect(m_socket, (sockaddr*)&addr, sizeof(addr));
        checkError(result, "connect");
    }

    // Send data
    void send(const std::string& data) 
    {
        int result = ::send(m_socket, data.c_str(), data.size(), 0);
        checkError(result, "send");
    }

    // Receive data
    std::string receive(size_t size) 
    {
        std::string buffer(size, '\0');
        ssize_t bytes_read = ::recv(m_socket, &buffer[0], size, 0);
        if (bytes_read == -1) 
        {
            throwError("receive");
        }
        buffer.resize(bytes_read);
        return buffer;
    }

private:
    socket_t m_socket;

    // Check result and throw exception if an error occurred
    void checkError(int result, const std::string& operation) 
    {
        if (result == -1) 
        {
            throwError(operation);
        }
    }

    // Throw an exception with platform-specific error details
    void throwError(const std::string& operation) 
    {
#ifdef _WIN32
        int err = WSAGetLastError();
        throw std::runtime_error(operation + " failed with error: " + std::to_string(err));
#else
        throw std::runtime_error(operation + " failed with error: " + std::string(strerror(errno)));
#endif
    }
};

#ifdef _WIN32
// Initialize Winsock once for the entire application
struct WinsockInitializer 
{
    WinsockInitializer() 
    {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) 
        {
            throw std::runtime_error("WSAStartup failed");
        }
    }

    ~WinsockInitializer() 
    {
        WSACleanup();
    }
};

// This creates a single static instance that initializes at program start
// and cleans up at program exit
static WinsockInitializer g_winsockInit;
#endif