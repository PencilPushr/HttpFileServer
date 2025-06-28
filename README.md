# HttpFileServer

Use this only as a learning device, as you will see below, this is very much a Work-In-Progress.

WIP: Needs several security features, backups and other features for more production ready code, but should be fine if you run this off a raspberry pi and on your local network.
Feel free to the steal code, it's what I did to make this.

## Version details
 - Version are as follows:
 - V1.0 The first iteration, it uses raw sockets, http, no protection and the underlying file system to manage the files.
 - V1.1 Uses the Socket wrapper classes to setup and communicate:
	-  Socket  ->  ``Socket.h/.cpp``
		-	Wrapper over regular socket to create a lightweight cross-platform socket class
	-  SocketStreambuf  ->  ``SocketStreambuf.h/.cpp``
		-	Custom stream buffer. 
		-   Used to turn raw sockets into a stream, provides an interface for stream types. 
			- Can be passed to iostream and used regularly like a stream.
	-  SocketStream  ->  ``SocketStream.h/cpp``
		-   Cleaner alternative to passing SocketStreambuf to std::iostream
		-   Custom stream, enables sockets to be used like streams via ``<<``,``>>``, ``std::getline`` ...

### SocketStreambuf example
```cpp
int main() 
{
    try
    {
        int socket = /* ... */;

        SocketStreambuf sock_buf(socket_fd);
        std::iostream io_stream(&sock_buf);

        io_stream << "Hello, Server!" << std::endl;
        io_stream.flush();

        std::string response;
        if (std::getline(io_stream, response)) 
        {
            std::cout << "Received: " << response << std::endl;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    return 0;
}
```

### SocketStream example
```cpp

int main() 
{
    try 
    {
        // Replace with your server's host and port 
        // Using the Socket wrapper class, but a raw socket works just as well
        Socket socket("localhost", "12345");

        SocketStream socket_stream(socket.getSocket());

        std::string line;

        // Write to the socket
        socket_stream << "Hello, Server!" << std::endl;

        // Read from the socket
        while (std::getline(socket_stream, line)) 
        {
            std::cout << "Received: " << line << std::endl;
        }

        if (!socket_stream.eof()) 
        {
            std::cerr << "Error reading from socket." << std::endl;
        }
    } 
    catch (const std::exception& e) 
    {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}

```