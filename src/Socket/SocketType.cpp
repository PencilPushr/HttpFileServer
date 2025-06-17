#include "SocketType.h"
#include <cstring>
#include <stdexcept>

Socket::Socket(const std::string& host, const std::string& port) : m_socket(INVALID_SOCKET)
{

#ifdef _WIN32
    // Initialize Winsock
    WSADATA wsaData;
    int wsaret = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsaret != 0)
    {
        /* Tell the user that we could not find a usable */
        /* WinSock DLL.*/
        printf("The Winsock dll not found!\n");
        throw std::runtime_error("WSAStartup failed");
    }
    else
    {
        printf("The Winsock dll found!\n");
        printf("The status: %s.\n", m_wsaData.szSystemStatus);
    }
#endif

    struct addrinfo hints{};
    struct addrinfo* res = nullptr;

    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;      // Allow IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;  // Stream socket
    hints.ai_protocol = IPPROTO_TCP;  // TCP protocol

    int err = getaddrinfo(host.c_str(), port.c_str(), &hints, &res);
    if (err != 0 || res == nullptr)
    {
        throw std::runtime_error("getaddrinfo failed");
    }

    m_socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (m_socket == INVALID_SOCKET)
    {
#ifdef _WIN32
        printf("Client: socket() - Error at socket(): %ld\n", WSAGetLastError());
        WSACleanup();
#else // POSIX
        printf("Client: socket() - Error at socket(): %s\n", strerror(errno));
#endif
        freeaddrinfo(res);
        throw std::runtime_error("socket creation failed");
    }

    if (connect(m_socket, res->ai_addr, res->ai_addrlen) == -1)
    {
        freeaddrinfo(res);
#ifdef _WIN32
        closeSocket(m_socket);
        WSACleanup();
#else
        closeSocket(m_socket);
#endif
        throw std::runtime_error("connect failed");
    }

    freeaddrinfo(res);
}

Socket::~Socket() 
{
    if (m_socket != INVALID_SOCKET) 
    {
#ifdef _WIN32
        closesocket(m_socket);
        WSACleanup();
#else
        close(m_socket);
#endif
    }
}
