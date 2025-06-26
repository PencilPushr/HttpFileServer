#pragma once
#include <string>
#include <thread>
#include <atomic>
#include "HTTP/HttpResponse.h"
#include "HTTP/HttpRequest.h"
#include "Config.h"
#include "FileManager.h"
#include "StaticFileServer.h"
#include "Logger.h"


// This will be replaced with Socket wrapper classes in V1.1
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

class Server
{
private:
    Config config;
    Logger logger;  // Initialize logger first
    int server_socket;
    FileManager file_manager;
    StaticFileServer static_server;
    std::atomic<bool> running{ false };

public:
    Server(const Config& cfg);
    ~Server();
    void start();
    void stop();

private:
    void handleClient(int client_socket, const std::string& client_ip);
    HttpResponse handleRequest(const HttpRequest& request);
    HttpResponse handleApiRequest(const HttpRequest& request);
    void initializeSocket();
    void cleanup();
};