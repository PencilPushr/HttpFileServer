#pragma once

#include <string>

#ifdef _WIN32

#include <WinSock2.h>
#include <ws2tcpip.h>
using socket_t = SOCKET;
inline int closeSocket(socket_t socket) { return closesocket(socket); }
#pragma comment(lib, "Ws2_32.lib")

#else // POSIX

#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#define INVALID_SOCKET (-1)
using socket_t = int;
inline int closeSocket(socket_t socket) { return close(socket); }

#endif

class Socket
{
public:
    Socket(const std::string& host, const std::string& port);
    ~Socket();

    socket_t GetSocket() const { return m_socket; }

private:
    socket_t m_socket;
};