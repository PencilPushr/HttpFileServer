#include "SocketType.h"
#include <cstring>
#include <stdexcept>

// Factory method to create a server socket
Socket createServerSocket(const std::string& address, int port, int backlog = 10) {
    Socket sock;
    sock.create(AF_INET, SOCK_STREAM, 0);
    sock.setReuseAddr();
    sock.bind(address, port);
    sock.listen(backlog);
    return sock;
}

// Factory method to create a client socket
Socket createClientSocket(const std::string& address, int port) {
    Socket sock;
    sock.create(AF_INET, SOCK_STREAM, 0);
    sock.connect(address, port);
    return sock;
}
