#include "Server.h"
#include <filesystem>
#include <stdexcept>
#include <iostream>

namespace fs = std::filesystem;

#ifdef _WIN32
#define close closesocket
typedef int ssize_t;
#endif

Server::Server(const Config& cfg)
    : config(cfg)
    , logger(cfg.logFile, cfg.enable_logging)  // Initialize logger first
    , server_socket(-1)
    , file_manager(cfg.rootDir, logger)
    , static_server(cfg.webDir, logger)
{
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        throw std::runtime_error("Failed to initialize Winsock");
    }
#endif
    initializeSocket();
}

Server::~Server() {
    cleanup();
}

void Server::initializeSocket() {
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        throw std::runtime_error("Failed to create socket");
    }

    int opt = 1;
#ifdef _WIN32
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR,
        reinterpret_cast<const char*>(&opt), sizeof(opt)) < 0) {
#else
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
#endif
        close(server_socket);
        throw std::runtime_error("Failed to set socket options");
    }
    }

void Server::start() {
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(config.port);

    if (bind(server_socket, (struct sockaddr*)&address, sizeof(address)) < 0) {
        throw std::runtime_error("Failed to bind socket");
    }

    if (listen(server_socket, 10) < 0) {
        throw std::runtime_error("Failed to listen on socket");
    }

    running = true;
    logger.info("File server starting on port " + std::to_string(config.port));
    logger.info("Serving files from: " + config.rootDir);
    logger.info("Serving web interface from: " + config.webDir);

    while (running) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);

        if (client_socket >= 0) {
            std::string client_ip = inet_ntoa(client_addr.sin_addr);
            std::thread(&Server::handleClient, this, client_socket, client_ip).detach();
        }
        else if (running) {
            logger.error("Failed to accept client connection");
        }
    }
}

void Server::stop() {
    running = false;
    if (server_socket >= 0) {
        close(server_socket);
        server_socket = -1;
    }
    logger.info("Server stopped");
}

void Server::cleanup() {
    stop();
#ifdef _WIN32
    WSACleanup();
#endif
}

void Server::handleClient(int client_socket, const std::string & client_ip) {
    char buffer[8192];
    ssize_t bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);

    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        HttpRequest request = HttpRequest::parse(std::string(buffer), client_ip);

        logger.info(client_ip + " " + request.method + " " + request.path);

        HttpResponse response = handleRequest(request);

        if (config.enable_cors) {
            response.headers["Access-Control-Allow-Origin"] = "*";
            response.headers["Access-Control-Allow-Methods"] = "GET, POST, PUT, DELETE, OPTIONS";
            response.headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization";
        }

        std::string response_str = response.serialize();
        send(client_socket, response_str.c_str(), static_cast<int>(response_str.length()), 0);
    }

    close(client_socket);
}

HttpResponse Server::handleRequest(const HttpRequest & request) {
    // Handle CORS preflight
    if (request.method == "OPTIONS") {
        HttpResponse response;
        response.status_code = 204;
        return response;
    }

    // API routes
    if (request.path.starts_with("/api/")) {
        return handleApiRequest(request);
    }

    // Serve static files (web interface)
    return static_server.serveFile(request.path);
}

HttpResponse Server::handleApiRequest(const HttpRequest & request) {
    HttpResponse response;

    if (request.path == "/api/files" && request.method == "GET") {
        auto params = request.parseQuery();
        std::string path = params.count("path") ? params.at("path") : "";

        auto files = file_manager.listDirectory(path);
        json json_files = json::array();

        for (const auto& file : files) {
            json_files.push_back(file.toJson());
        }

        response.setJson(json_files);
    }
    else if (request.path == "/api/delete" && request.method == "DELETE") {
        auto params = request.parseQuery();
        if (!params.count("file")) {
            response.setError(400, "Missing file parameter");
            return response;
        }

        bool success = file_manager.deleteFile(params.at("file"));
        if (success) {
            json result = {
                {"success", true},
                {"message", "File deleted successfully"}
            };
            response.setJson(result);
        }
        else {
            response.setError(404, "File not found or could not be deleted");
        }
    }
    else if (request.path.starts_with("/api/download") && request.method == "GET") {
        auto params = request.parseQuery();
        if (!params.count("file")) {
            response.setError(400, "Missing file parameter");
            return response;
        }

        auto fileData = file_manager.readFile(params.at("file"));
        if (fileData.empty()) {
            response.setError(404, "File not found");
            return response;
        }

        // Set appropriate headers for file download
        response.body = fileData;
        response.headers["Content-Type"] = "application/octet-stream";
        response.headers["Content-Disposition"] = "attachment; filename=\"" +
            fs::path(params.at("file")).filename().string() + "\"";
        response.status_code = 200;
    }
    else if (request.path == "/api/upload" && request.method == "POST") {
        // TODO: Implement multipart form parsing
        response.setError(501, "Upload not yet implemented");
    }
    else if (request.path == "/api/stats" && request.method == "GET") {
        response.setJson(file_manager.getStats());
    }
    else {
        response.setError(404, "API endpoint not found");
    }

    return response;
}