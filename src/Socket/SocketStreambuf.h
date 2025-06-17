// SocketStreambuf.h
#pragma once

#include <streambuf>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
    typedef SOCKET socket_t;
#else
#include <unistd.h>
typedef int socket_t;
#endif

class SocketStreambuf : public std::streambuf
{
public:
    explicit SocketStreambuf(socket_t socket_fd, std::size_t buff_sz = 8192);

protected:
    // Input buffer operations
    virtual int_type underflow() override;

    // Output buffer operations
    virtual int_type overflow(int_type ch) override;

    // Flush the output buffer
    virtual int sync() override;

private:
    socket_t m_socket;
    std::vector<char> m_in_buffer;
    std::vector<char> m_out_buffer;
};

// David -> What would be ideal is to have a thread for reading and writing, so that you can have independent sends and reads
// This could be done with a Writer Object thread using Datastructure (Queue) that is notified.
// Have a buffering policy for when it gets full