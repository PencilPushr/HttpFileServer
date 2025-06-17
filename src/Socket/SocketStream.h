#pragma once

#include "SocketType.h"
#include "SocketStreambuf.h"
#include <iostream>

class SocketStream : public std::iostream
{
public:
    explicit SocketStream(Socket& socket);
    ~SocketStream() override;

private:
    SocketStreambuf m_sockbuf;
};
