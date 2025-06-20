#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#include <mutex>
#include <regex>
#include <chrono>
#include <iomanip>
#include <json/json.h>  // You'll need jsoncpp library

// Socket includes (Unix/Linux)
//#include <sys/socket.h>
//#include <netinet/in.h>
//#include <unistd.h>
//#include <arpa/inet.h>
#include "Socket/SocketStream.h"

namespace fs = std::filesystem;

class Logger {
private:
    std::ofstream log_file;
    std::mutex log_mutex;
    bool enabled;

public:
    Logger(const std::string& filename, bool enable = true) : enabled(enable) {
        if (enabled) {
            log_file.open(filename, std::ios::app);
        }
    }

    void log(const std::string& level, const std::string& message) {
        if (!enabled) return;

        std::lock_guard<std::mutex> lock(log_mutex);
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);

        log_file << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S")
            << "] [" << level << "] " << message << std::endl;
        log_file.flush();

        // Also print to console
        std::cout << "[" << level << "] " << message << std::endl;
    }

    void info(const std::string& msg) { log("INFO", msg); }
    void error(const std::string& msg) { log("ERROR", msg); }
    void warning(const std::string& msg) { log("WARN", msg); }
};

class HttpRequest {
public:
    std::string method;
    std::string path;
    std::string query_string;
    std::map<std::string, std::string> headers;
    std::vector<uint8_t> body;
    std::string client_ip;

    static HttpRequest parse(const std::string& raw_request, const std::string& client_ip = "") {
        HttpRequest req;
        req.client_ip = client_ip;
        std::istringstream stream(raw_request);
        std::string line;

        // Parse request line
        if (std::getline(stream, line)) {
            std::istringstream request_line(line);
            std::string full_path;
            request_line >> req.method >> full_path;

            // Split path and query string
            size_t query_pos = full_path.find('?');
            if (query_pos != std::string::npos) {
                req.path = full_path.substr(0, query_pos);
                req.query_string = full_path.substr(query_pos + 1);
            }
            else {
                req.path = full_path;
            }
        }

        // Parse headers
        while (std::getline(stream, line) && line != "\r") {
            size_t colon = line.find(':');
            if (colon != std::string::npos) {
                std::string key = line.substr(0, colon);
                std::string value = line.substr(colon + 2);
                if (!value.empty() && value.back() == '\r') {
                    value.pop_back();
                }
                req.headers[key] = value;
            }
        }

        // Read body if Content-Length is specified
        auto content_length_it = req.headers.find("Content-Length");
        if (content_length_it != req.headers.end()) {
            int content_length = std::stoi(content_length_it->second);
            std::string remaining_data((std::istreambuf_iterator<char>(stream)),
                std::istreambuf_iterator<char>());
            req.body = std::vector<uint8_t>(remaining_data.begin(), remaining_data.end());
        }

        return req;
    }

    std::map<std::string, std::string> parseQuery() const {
        std::map<std::string, std::string> params;
        std::istringstream stream(query_string);
        std::string pair;

        while (std::getline(stream, pair, '&')) {
            size_t equals = pair.find('=');
            if (equals != std::string::npos) {
                params[pair.substr(0, equals)] = urlDecode(pair.substr(equals + 1));
            }
        }
        return params;
    }

private:
    std::string urlDecode(const std::string& str) const {
        std::string result;
        for (size_t i = 0; i < str.length(); ++i) {
            if (str[i] == '%' && i + 2 < str.length()) {
                int value = std::stoi(str.substr(i + 1, 2), nullptr, 16);
                result += static_cast<char>(value);
                i += 2;
            }
            else if (str[i] == '+') {
                result += ' ';
            }
            else {
                result += str[i];
            }
        }
        return result;
    }
};

class HttpResponse {
public:
    int status_code = 200;
    std::map<std::string, std::string> headers;
    std::vector<uint8_t> body;

    HttpResponse() {
        // Default security headers
        headers["X-Frame-Options"] = "DENY";
        headers["X-Content-Type-Options"] = "nosniff";
        headers["X-XSS-Protection"] = "1; mode=block";
    }

    std::string serialize() const {
        std::ostringstream response;

        // Status line
        response << "HTTP/1.1 " << status_code << " " << getStatusText() << "\r\n";

        // Headers
        auto headers_copy = headers;
        headers_copy["Content-Length"] = std::to_string(body.size());
        headers_copy["Server"] = "RaspberryPi-FileServer/1.0";

        for (const auto& [key, value] : headers_copy) {
            response << key << ": " << value << "\r\n";
        }
        response << "\r\n";

        std::string header_str = response.str();
        std::vector<uint8_t> full_response(header_str.begin(), header_str.end());
        full_response.insert(full_response.end(), body.begin(), body.end());

        return std::string(full_response.begin(), full_response.end());
    }

    void setJson(const Json::Value& json) {
        Json::StreamWriterBuilder builder;
        std::string json_str = Json::writeString(builder, json);
        body = std::vector<uint8_t>(json_str.begin(), json_str.end());
        headers["Content-Type"] = "application/json";
    }

    void setError(int code, const std::string& message) {
        status_code = code;
        Json::Value error;
        error["error"] = message;
        error["status"] = code;
        setJson(error);
    }

private:
    std::string getStatusText() const {
        switch (status_code) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 409: return "Conflict";
        case 413: return "Payload Too Large";
        case 500: return "Internal Server Error";
        default: return "Unknown";
        }
    }
};

class FileManager {
private:
    std::string root_directory;
    std::mutex file_mutex;
    Logger& logger;

public:
    struct FileInfo {
        std::string name;
        std::string path;
        uint64_t size;
        std::string modified;
        bool is_directory;
        std::string mime_type;

        Json::Value toJson() const {
            Json::Value obj;
            obj["name"] = name;
            obj["path"] = path;
            obj["size"] = static_cast<Json::UInt64>(size);
            obj["modified"] = modified;
            obj["is_directory"] = is_directory;
            obj["mime_type"] = mime_type;
            obj["size_formatted"] = formatFileSize(size);
            return obj;
        }
    };

    FileManager(const std::string& root, Logger& log) : root_directory(root), logger(log) {
        fs::create_directories(root);
        logger.info("FileManager initialized with root: " + root);
    }

    std::vector<FileInfo> listDirectory(const std::string& relative_path = "") {
        std::lock_guard<std::mutex> lock(file_mutex);
        std::vector<FileInfo> files;

        std::string full_path = root_directory + "/" + relative_path;
        if (!isPathSafe(full_path)) {
            logger.warning("Unsafe path access attempt: " + relative_path);
            return files;
        }

        try {
            for (const auto& entry : fs::directory_iterator(full_path)) {
                FileInfo info;
                info.name = entry.path().filename().string();
                info.path = relative_path + (relative_path.empty() ? "" : "/") + info.name;
                info.is_directory = entry.is_directory();
                info.size = entry.is_directory() ? 0 : entry.file_size();
                info.mime_type = getMimeType(info.name);

                auto time = fs::last_write_time(entry);
                auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    time - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
                std::time_t tt = std::chrono::system_clock::to_time_t(sctp);
                std::ostringstream oss;
                oss << std::put_time(std::localtime(&tt), "%Y-%m-%d %H:%M:%S");
                info.modified = oss.str();

                files.push_back(info);
            }
        }
        catch (const std::exception& e) {
            logger.error("Error listing directory: " + std::string(e.what()));
        }

        return files;
    }

    std::vector<uint8_t> readFile(const std::string& relative_path) {
        std::lock_guard<std::mutex> lock(file_mutex);
        std::string full_path = root_directory + "/" + relative_path;

        if (!isPathSafe(full_path)) {
            logger.warning("Unsafe file read attempt: " + relative_path);
            return {};
        }

        std::ifstream file(full_path, std::ios::binary);
        if (!file) {
            logger.warning("File not found: " + relative_path);
            return {};
        }

        logger.info("File downloaded: " + relative_path);
        return std::vector<uint8_t>((std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>());
    }

    bool writeFile(const std::string& relative_path, const std::vector<uint8_t>& data) {
        std::lock_guard<std::mutex> lock(file_mutex);
        std::string full_path = root_directory + "/" + relative_path;

        if (!isPathSafe(full_path)) {
            logger.warning("Unsafe file write attempt: " + relative_path);
            return false;
        }

        // Create directory if needed
        fs::create_directories(fs::path(full_path).parent_path());

        std::ofstream file(full_path, std::ios::binary);
        if (!file) {
            logger.error("Failed to create file: " + relative_path);
            return false;
        }

        file.write(reinterpret_cast<const char*>(data.data()), data.size());
        bool success = file.good();

        if (success) {
            logger.info("File uploaded: " + relative_path + " (" + std::to_string(data.size()) + " bytes)");
        }
        else {
            logger.error("Failed to write file: " + relative_path);
        }

        return success;
    }

    bool deleteFile(const std::string& relative_path) {
        std::lock_guard<std::mutex> lock(file_mutex);
        std::string full_path = root_directory + "/" + relative_path;

        if (!isPathSafe(full_path)) {
            logger.warning("Unsafe file delete attempt: " + relative_path);
            return false;
        }

        bool success = fs::remove_all(full_path) > 0;

        if (success) {
            logger.info("File deleted: " + relative_path);
        }
        else {
            logger.warning("Failed to delete file: " + relative_path);
        }

        return success;
    }

    Json::Value getStats() {
        std::lock_guard<std::mutex> lock(file_mutex);
        Json::Value stats;

        uint64_t total_size = 0;
        int file_count = 0;
        int folder_count = 0;

        try {
            for (const auto& entry : fs::recursive_directory_iterator(root_directory)) {
                if (entry.is_directory()) {
                    folder_count++;
                }
                else {
                    file_count++;
                    total_size += entry.file_size();
                }
            }
        }
        catch (const std::exception& e) {
            logger.error("Error calculating stats: " + std::string(e.what()));
        }

        stats["total_files"] = file_count;
        stats["total_folders"] = folder_count;
        stats["total_size"] = static_cast<Json::UInt64>(total_size);
        stats["total_size_formatted"] = formatFileSize(total_size);

        return stats;
    }

private:
    bool isPathSafe(const std::string& path) const {
        try {
            fs::path canonical_root = fs::canonical(fs::absolute(root_directory));
            fs::path canonical_path = fs::canonical(fs::absolute(path));

            auto rel = fs::relative(canonical_path, canonical_root);
            return !rel.string().starts_with("..");
        }
        catch (const std::exception&) {
            // If canonicalization fails, assume unsafe
            return false;
        }
    }

    std::string getMimeType(const std::string& filename) const {
        std::string ext = fs::path(filename).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        static std::map<std::string, std::string> mime_types = {
            {".html", "text/html"}, {".css", "text/css"}, {".js", "application/javascript"},
            {".json", "application/json"}, {".txt", "text/plain"}, {".pdf", "application/pdf"},
            {".jpg", "image/jpeg"}, {".jpeg", "image/jpeg"}, {".png", "image/png"},
            {".gif", "image/gif"}, {".mp4", "video/mp4"}, {".mp3", "audio/mpeg"},
            {".zip", "application/zip"}, {".xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"}
        };

        auto it = mime_types.find(ext);
        return it != mime_types.end() ? it->second : "application/octet-stream";
    }

    static std::string formatFileSize(uint64_t size) {
        const char* units[] = { "B", "KB", "MB", "GB", "TB" };
        int unit = 0;
        double s = size;

        while (s >= 1024 && unit < 4) {
            s /= 1024;
            unit++;
        }

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << s << " " << units[unit];
        return oss.str();
    }
};

class StaticFileServer {
private:
    std::string web_directory;
    Logger& logger;

public:
    StaticFileServer(const std::string& web_dir, Logger& log)
        : web_directory(web_dir), logger(log) {}

    HttpResponse serveFile(const std::string& path) {
        HttpResponse response;

        std::string file_path = web_directory + path;
        if (path == "/") {
            file_path += "/index.html";
        }

        std::ifstream file(file_path, std::ios::binary);
        if (!file) {
            response.status_code = 404;
            response.setError(404, "File not found");
            return response;
        }

        std::vector<uint8_t> content((std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>());

        response.body = content;
        response.headers["Content-Type"] = getMimeType(file_path);
        response.headers["Cache-Control"] = "no-cache";

        return response;
    }

private:
    std::string getMimeType(const std::string& filename) {
        std::string ext = fs::path(filename).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        static std::map<std::string, std::string> mime_types = {
            {".html", "text/html"}, {".css", "text/css"}, {".js", "application/javascript"},
            {".png", "image/png"}, {".jpg", "image/jpeg"}, {".gif", "image/gif"},
            {".svg", "image/svg+xml"}, {".ico", "image/x-icon"}
        };

        auto it = mime_types.find(ext);
        return it != mime_types.end() ? it->second : "text/plain";
    }
};

class Server {
private:
    Config config;
    int server_socket;
    FileManager file_manager;
    StaticFileServer static_server;
    Logger logger;
    bool running = false;

public:
    FileServer(const Config& cfg)
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

    void stop() {
        running = false;
        close(server_socket);
        logger.info("Server stopped");
    }

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