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


#include "Socket/SocketStream.h"

class Server
{
private:
    Config config;
    Logger logger;  // Initialize logger first
    Socket m_socket;
    FileManager file_manager;
    StaticFileServer static_server;
    std::atomic<bool> running{ false };

    // Route handler type
    using RouteHandler = std::function<HttpResponse(const HttpRequest&, Server&)>;

    // Route table: key is "METHOD /path"
    std::unordered_map<std::string, RouteHandler> routes;

public:
    Server(const Config& cfg);
    ~Server();
    void start();
    void stop();

private:
    void handleClient(Socket client_socket, const std::string& client_ip);
    HttpResponse handleRequest(const HttpRequest& request);
    HttpResponse handleApiRequest(const HttpRequest& request);
    void initializeSocket();
    void setupRoutes();

    // Individual route handlers
    HttpResponse handleGetFiles(const HttpRequest& request);
    HttpResponse handleDeleteFile(const HttpRequest& request);
    HttpResponse handleDownloadFile(const HttpRequest& request);
    HttpResponse handleUploadFile(const HttpRequest& request);
    HttpResponse handleGetStats(const HttpRequest& request);
    HttpResponse handleSearch(const HttpRequest& request);
};