#pragma once

class Server {
private:
    Config config;
    int server_socket;
    FileManager file_manager;
    StaticFileServer static_server;
    Logger logger;
    bool running = false;

public:
    Server(const Config& cfg)
        : config(cfg),
        file_manager(cfg.root_directory, logger),
        static_server(cfg.web_directory, logger),
        logger(cfg.log_file, cfg.enable_logging) {}

    void start() {
        Socket socket(config.port);
        SocketStream sockstream(socket);

        int opt = 1;
        setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

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
        logger.info("Serving files from: " + config.root_directory);
        logger.info("Serving web interface from: " + config.web_directory);

        while (running) {
            sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);
            int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);

            if (client_socket >= 0) {
                std::string client_ip = inet_ntoa(client_addr.sin_addr);
                std::thread(&FileServer::handleClient, this, client_socket, client_ip).detach();
            }
        }
    }

    void stop();

private:
    void handleClient(int client_socket, const std::string& client_ip) {
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
            send(client_socket, response_str.c_str(), response_str.length(), 0);
        }

        close(client_socket);
    }

    HttpResponse handleRequest(const HttpRequest& request) {
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

    HttpResponse handleApiRequest(const HttpRequest& request) {
        HttpResponse response;

        if (request.path == "/api/files" && request.method == "GET") {
            auto params = request.parseQuery();
            std::string path = params.count("path") ? params.at("path") : "";

            auto files = file_manager.listDirectory(path);
            Json::Value json_files(Json::arrayValue);

            for (const auto& file : files) {
                json_files.append(file.toJson());
            }

            response.setJson(json_files);

        }
        else if (request.path == "/api/download" && request.method == "GET") {
            auto params = request.parseQuery();
            if (!params.count("file")) {
                response.setError(400, "Missing file parameter");
                return response;
            }

            auto file_data = file_manager.readFile(params.at("file"));
            if (file_data.empty()) {
                response.setError(404, "File not found");
                return response;
            }

            response.body = file_data;
            response.headers["Content-Type"] = "application/octet-stream";
            response.headers["Content-Disposition"] = "attachment; filename=\"" +
                fs::path(params.at("file")).filename().string() + "\"";

        }
        else if (request.path == "/api/upload" && request.method == "POST") {
            // TODO: Implement multipart form parsing
            response.setError(501, "Upload not yet implemented");

        }
        else if (request.path == "/api/delete" && request.method == "DELETE") {
            auto params = request.parseQuery();
            if (!params.count("file")) {
                response.setError(400, "Missing file parameter");
                return response;
            }

            bool success = file_manager.deleteFile(params.at("file"));
            if (success) {
                Json::Value result;
                result["success"] = true;
                result["message"] = "File deleted successfully";
                response.setJson(result);
            }
            else {
                response.setError(404, "File not found or could not be deleted");
            }

        }
        else if (request.path == "/api/stats" && request.method == "GET") {
            response.setJson(file_manager.getStats());

        }
        else {
            response.setError(404, "API endpoint not found");
        }

        return response;
    }
};