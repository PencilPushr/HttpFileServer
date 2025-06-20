#pragma once

class Server 
{
public:
    FileServer(const Config& cfg);
    void start();
    void stop();

private:
    Config config;
    int server_socket;
    FileManager file_manager;
    StaticFileServer static_server;
    Logger logger;
    bool running = false;

    void handleClient(int client_socket, const std::string& client_ip);
    HttpResponse handleRequest(const HttpRequest& request);
    HttpResponse handleApiRequest(const HttpRequest& request);
};