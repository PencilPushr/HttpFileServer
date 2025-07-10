#include "SocketType.h"
#include <cstring>
#include <stdexcept>

// Factory method to create a server socket
Socket Socket::createServerSocket(const std::string& address, int port, int backlog) 
{
    Socket sock;
    sock.create(AF_INET, SOCK_STREAM, 0);
    sock.setReuseAddr();
    sock.bind(address, port);
    sock.listen(backlog);
    return sock;
}

// Factory method to create a client socket
Socket Socket::createClientSocket(const std::string& address, int port) 
{
    Socket sock;
    sock.create(AF_INET, SOCK_STREAM, 0);
    sock.connect(address, port);
    return sock;
}

Socket::Socket(int family, int type, int protocol)
{
    create(family, type, protocol);
}

Socket::Socket(socket_t sock) 
    : m_socket{sock}
{
    if (m_socket == INVALID_SOCKET)
    {
        throw std::runtime_error("Invalid socket descriptor");
    }
}

Socket::~Socket()
{
    if (m_socket != INVALID_SOCKET)
    {
        closeSocket(m_socket);
    }
}

Socket::Socket(Socket&& other) noexcept 
    : m_socket{ other.m_socket }
{
    other.m_socket = INVALID_SOCKET;
}

socket_t Socket::getSocket() const
{
    return m_socket;
}

Socket& Socket::operator=(Socket&& other) noexcept
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

void Socket::create(int family, int type, int protocol)
{
    m_socket = socket(family, type, protocol);
    if (m_socket == INVALID_SOCKET)
    {
        throwError("socket creation");
    }
}

void Socket::close()
{
    if (m_socket != INVALID_SOCKET)
    {
        closeSocket(m_socket);
        m_socket = INVALID_SOCKET;
    }
}

void Socket::bind(const std::string& address, int port)
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

void Socket::listen(int backlog)
{
    int result = ::listen(m_socket, backlog);
    checkError(result, "listen");
}

Socket Socket::accept()
{
    sockaddr_in client_addr{};
    socklen_t clientLen = sizeof(client_addr);
    socket_t clientSocket = ::accept(m_socket, (sockaddr*)&client_addr, &clientLen);

    if (clientSocket == INVALID_SOCKET)
    {
        throwError("accept");
    }

    return Socket(clientSocket);
}

void Socket::connect(const std::string& address, int port)
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

void Socket::send(const std::string& data)
{
    int result = ::send(m_socket, data.c_str(), data.size(), 0);
    checkError(result, "send");
}

std::string Socket::receive(size_t size)
{
    std::string buffer(size, '\0');
    ssize_t bytesRead = ::recv(m_socket, &buffer[0], size, 0);
    if (bytesRead == -1)
    {
        throwError("receive");
    }
    buffer.resize(bytesRead);
    return buffer;
}

// Get client address from accepted socket:
//      void Server::handleClient(Socket client) 
//      {
//          std::string clientIP = client.getRemoteAddress();
//          logger.info("New connection from: " + clientIP);
//          
//          // ... rest of handling ...
//      }
std::string Socket::getRemoteAddress() const
{
    sockaddr_storage addr{};
    socklen_t len = sizeof(addr);

    if (getpeername(m_socket, (sockaddr*)&addr, &len) == -1) 
    {
        return "unknown";
    }

    char ip[INET6_ADDRSTRLEN];
    if (addr.ss_family == AF_INET) 
    {
        inet_ntop(AF_INET, &((sockaddr_in*)&addr)->sin_addr, ip, sizeof(ip));
    }
    else if (addr.ss_family == AF_INET6) 
    {
        inet_ntop(AF_INET6, &((sockaddr_in6*)&addr)->sin6_addr, ip, sizeof(ip));
    }

    return std::string(ip);
}

std::string Socket::getLocalAddress() const
{
    sockaddr_storage addr{};
    socklen_t len = sizeof(addr);

    if (getsockname(m_socket, (sockaddr*)&addr, &len) == -1) {
        return "unknown";
    }

    char ip[INET6_ADDRSTRLEN];
    if (addr.ss_family == AF_INET)
    {
        inet_ntop(AF_INET, &((sockaddr_in*)&addr)->sin_addr, ip, sizeof(ip));
    }
    else if (addr.ss_family == AF_INET6)
    {
        inet_ntop(AF_INET6, &((sockaddr_in6*)&addr)->sin6_addr, ip, sizeof(ip));
    }

    return std::string(ip);
}

int Socket::getLocalPort() const
{
    sockaddr_storage addr{};
    socklen_t len = sizeof(addr);

    if (getsockname(m_socket, (sockaddr*)&addr, &len) == -1) {
        return -1;
    }

    if (addr.ss_family == AF_INET)
    {
        return ntohs(((sockaddr_in*)&addr)->sin_port);
    }
    else if (addr.ss_family == AF_INET6)
    {
        return ntohs(((sockaddr_in6*)&addr)->sin6_port);
    }

    return -1;
}

int Socket::getRemotePort() const
{
    sockaddr_storage addr{};
    socklen_t len = sizeof(addr);

    if (getpeername(m_socket, (sockaddr*)&addr, &len) == -1) 
    {
        return -1;
    }

    if (addr.ss_family == AF_INET)
    {
        return ntohs(((sockaddr_in*)&addr)->sin_port);
    }
    else if (addr.ss_family == AF_INET6)
    {
        return ntohs(((sockaddr_in6*)&addr)->sin6_port);
    }

    return -1;
}

bool Socket::isConnected() const
{
    sockaddr_storage addr{};
    socklen_t len = sizeof(addr);

    if (getpeername(m_socket, (sockaddr*)&addr, &len) == 0) 
    {
        return true;
    }

    return false;
}


/* 

    Private methods 

*/


void Socket::setReuseAddr()
{
    int opt = 1;
    int result = setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
    checkError(result, "setsockopt SO_REUSEADDR");
}

void Socket::checkError(int result, const std::string& operation)
{
    if (result == -1)
    {
        throwError(operation);
    }
}

void Socket::throwError(const std::string& operation)
{
#ifdef _WIN32
    int err = WSAGetLastError();
    throw std::runtime_error(operation + " failed with error: " + std::to_string(err));
#else
    throw std::runtime_error(operation + " failed with error: " + std::string(strerror(errno)));
#endif
}
