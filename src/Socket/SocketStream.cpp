#include "SocketStream.h"

SocketStream::SocketStream(Socket& socket)
        : std::iostream(nullptr)
        , m_sockbuf(socket.GetSocket())
{
    rdbuf(&m_sockbuf);
}

SocketStream::~SocketStream()
{
    // No need to delete m_sockbuf since it's not dynamically allocated
}
