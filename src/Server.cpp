#include "Server.h"
#include <filesystem>
#include <stdexcept>
#include <iostream>

#include "Multipartparser.h"

namespace fs = std::filesystem;

Server::Server(const Config& cfg)
    : config(cfg)
    , logger(cfg.logFile, cfg.enable_logging)  // Initialize logger first
    , file_manager(cfg.rootDir, logger)
    , static_server(cfg.webDir, logger)
{
    initializeSocket();
    setupRoutes();
}

Server::~Server() 
{
    stop();
}

void Server::initializeSocket() 
{
    try 
    {
        m_socket = Socket::createServerSocket("0.0.0.0", config.port);
    }
    catch (const std::exception& e) 
    {
        throw std::runtime_error("Failed to initialize server socket: " + std::string(e.what()));
    }
}

void Server::setupRoutes()
{
    // Register API routes with lambda wrappers to bind 'this'
    routes["GET /api/files"] = [this](const HttpRequest& req, Server&) { return handleGetFiles(req); };
    routes["DELETE /api/delete"] = [this](const HttpRequest& req, Server&) { return handleDeleteFile(req); };
    routes["GET /api/download"] = [this](const HttpRequest& req, Server&) { return handleDownloadFile(req); };
    routes["POST /api/upload"] = [this](const HttpRequest& req, Server&) { return handleUploadFile(req); };
    routes["GET /api/stats"] = [this](const HttpRequest& req, Server&) { return handleGetStats(req); };
    routes["GET /api/search"] = [this](const HttpRequest& req, Server&) { return handleSearch(req); };
}

void Server::start() 
{
    running = true;
    logger.info("File server starting on port " + std::to_string(config.port));
    logger.info("Serving files from: " + config.rootDir);
    logger.info("Serving web interface from: " + config.webDir);
    logger.info("\n");

    while (running) 
    {
        try 
        {
            // Accept returns a Socket object now
            Socket client_socket = m_socket.accept();

            if (running) 
            {
                // Get client IP from the Socket object
                std::string client_ip = client_socket.getRemoteAddress();

                logger.info("Client connected from " + std::string(client_ip));

                // Move the socket into the thread
                std::thread(&Server::handleClient, this, std::move(client_socket), client_ip).detach();
            }
        }
        catch (const std::exception& e) 
        {
            if (running) {
                logger.error("Failed to accept client connection: " + std::string(e.what()));
            }
        }
    }
}

void Server::stop() 
{
    running = false;
    m_socket.close();
    logger.info("Server stopped");
}

void Server::handleClient(Socket client_socket, const std::string& client_ip)
{
    try 
    {
        // Create a SocketStream for easier I/O
        SocketStream stream(client_socket);

        // Read the HTTP request using the stream
        std::string request_line;
        std::string request_data;

        // Read until we find the end of headers (empty line)
        bool headers_complete = false;
        while (std::getline(stream, request_line)) 
        {
            // Remove carriage return if present
            if (!request_line.empty() && request_line.back() == '\r') 
            {
                request_line.pop_back();
            }

            // Check for end of headers
            if (request_line.empty()) 
            {
                headers_complete = true;
                break;
            }

            request_data += request_line + "\n";
        }

        if (!headers_complete || request_data.empty()) 
        {
            logger.error("Invalid HTTP request from " + client_ip);
            return;
        }

        // Parse the request
        HttpRequest request = HttpRequest::parse(request_data, client_ip);

        // If this is a POST/PUT request, we might need to read the body
        if (request.method == "POST" || request.method == "PUT") 
        {
            // Look for Content-Length header
            auto it = request.headers.find("Content-Length");
            if (it != request.headers.end()) 
            {
                int content_length = std::stoi(it->second);
                if (content_length > 0) 
                {
                    request.body.resize(content_length);
                    stream.read((char*)request.body.data(), content_length);
                    if (!stream) 
                    {
                        logger.error("Failed to read request body from " + client_ip);
                        return;
                    }
                }
            }
        }

        logger.info(client_ip + " " + request.method + " " + request.path);

        // Handle the request
        HttpResponse response = handleRequest(request);

        // Add CORS headers if enabled
        if (config.enable_cors) {
            response.headers["Access-Control-Allow-Origin"] = "*";
            response.headers["Access-Control-Allow-Methods"] = "GET, POST, PUT, DELETE, OPTIONS";
            response.headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization";
        }

        // Send the response using the stream
        std::string response_str = response.serialize();
        stream << response_str;
        stream.flush();

        // The Socket destructor will close the connection
    }
    catch (const std::exception& e) {
        logger.error("Error handling client " + client_ip + ": " + std::string(e.what()));
    }
}

HttpResponse Server::handleRequest(const HttpRequest & request) 
{
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

HttpResponse Server::handleApiRequest(const HttpRequest& request)
{
    // For routes with dynamic segments (like /api/download?file=...), 
    // we need to match the base path
    std::string route_key = request.method + " " + request.path;

    // Try exact match first
    auto it = routes.find(route_key);
    if (it != routes.end()) {
        return it->second(request, *this);
    }

    // For paths with query strings or dynamic segments, match the base path
    size_t query_pos = request.path.find('?');
    if (query_pos != std::string::npos) 
    {
        std::string base_path = request.path.substr(0, query_pos);
        route_key = request.method + " " + base_path;
        it = routes.find(route_key);
        if (it != routes.end()) 
        {
            return it->second(request, *this);
        }
    }

    // No route found
    HttpResponse response;
    response.setError(404, "API endpoint not found");
    return response;
}

HttpResponse Server::handleGetFiles(const HttpRequest& request)
{
    HttpResponse response;
    auto params = request.parseQuery();
    std::string path = params.count("path") ? params.at("path") : "";

    auto files = file_manager.listDirectory(path);
    json json_files = json::array();

    for (const auto& file : files) {
        json_files.push_back(file.toJson());
    }

    response.setJson(json_files);
    return response;
}

HttpResponse Server::handleDeleteFile(const HttpRequest& request)
{
    HttpResponse response;
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

    return response;
}

HttpResponse Server::handleDownloadFile(const HttpRequest& request)
{
    HttpResponse response;
    auto params = request.parseQuery();

    if (!params.count("file")) 
    {
        response.setError(400, "Missing file parameter");
        return response;
    }

    auto fileData = file_manager.readFile(params.at("file"));
    if (fileData.empty()) 
    {
        response.setError(404, "File not found");
        return response;
    }

    response.body = fileData;
    response.headers["Content-Type"] = "application/octet-stream";
    response.headers["Content-Disposition"] = "attachment; filename=\"" +
        fs::path(params.at("file")).filename().string() + "\"";
    response.status_code = 200;

    return response;
}

HttpResponse Server::handleGetStats(const HttpRequest& request)
{
    HttpResponse response;
    response.setJson(file_manager.getStats());
    return response;
}

HttpResponse Server::handleSearch(const HttpRequest& request)
{
  
    auto params = request.parseQuery();
    std::string query = params["query"];
    std::string path = params.count("path") ? params["path"] : ""; // Default to root or current path
    bool recursive = params.count("recursive") && params["recursive"] == "true"; // Default to false
    std::string type = params.count("type") ? params["type"] : "both"; // Default to both files and dirs

    if (query.empty()) 
    {
        HttpResponse response;
        response.setError(400, "Missing query parameter");
        return response;
    }

    auto results = file_manager.searchFiles(query, path, recursive, type);
    json json_results = json::array();
    for (const auto& file : results) 
    {
        json_results.push_back(file.toJson());
    }

    HttpResponse response;
    response.setJson(json_results);
    return response;
}

HttpResponse Server::handleUploadFile(const HttpRequest& request)
{
    HttpResponse response;

    // Check Content-Type header
    auto ct_it = request.headers.find("Content-Type");
    if (ct_it == request.headers.end()) 
    {
        response.setError(400, "Missing Content-Type header");
        return response;
    }

    // Check total request size
    if (request.body.size() > config.max_upload_size) 
    {
        response.setError(413, "Request entity too large. Max size: " +
            std::to_string(config.max_upload_size) + " bytes");
        return response;
    }

    // Parse Content-Type to extract boundary
    std::string content_type = ct_it->second;
    size_t boundary_pos = content_type.find("boundary=");
    if (boundary_pos == std::string::npos) 
    {
        response.setError(400, "Missing boundary in Content-Type");
        return response;
    }

    std::string boundary = content_type.substr(boundary_pos + 9);
    if (!boundary.empty() && boundary.front() == '"' && boundary.back() == '"') 
    {
        boundary = boundary.substr(1, boundary.length() - 2);
    }

    // Parse multipart data
    std::string body_str(request.body.begin(), request.body.end());
    auto parts = MultipartParser::parse(body_str, boundary);

    // Count file parts
    size_t file_count = std::count_if(parts.begin(), parts.end(),
        [](const auto& part) { return part.isFile(); });

    if (file_count == 0) 
    {
        response.setError(400, "No files found in upload");
        return response;
    }

    if (file_count > config.max_uploads_per_request) 
    {
        response.setError(400, "Too many files. Max allowed: " +
            std::to_string(config.max_uploads_per_request));
        return response;
    }

    json uploaded_files = json::array();
    std::vector<std::string> errors;

    // Create temp directory if it doesn't exist
    std::filesystem::create_directories(config.upload_temp_dir);

    for (const auto& part : parts) 
    {
        if (!part.isFile()) continue;

        try 
        {
            // Validate filename
            if (part.filename.empty()) 
            {
                errors.push_back("Empty filename not allowed");
                continue;
            }

            // Security: Remove any path components from filename
            std::filesystem::path safe_filename = std::filesystem::path(part.filename).filename();

            // Check for directory traversal attempts
            if (safe_filename.string().find("..") != std::string::npos) 
            {
                errors.push_back("Invalid filename: " + part.filename);
                continue;
            }

            // Validate extension
            if (!config.allowed_extensions.empty()) 
            {
                std::string ext = safe_filename.extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                bool allowed = std::find(
                        config.allowed_extensions.begin(),
                        config.allowed_extensions.end(), 
                        ext)
                        != config.allowed_extensions.end();

                if (!allowed) 
                {
                    errors.push_back("File type not allowed: " + ext);
                    continue;
                }
            }

            // Validate file size
            if (part.data.size() > config.max_upload_size) 
            {
                errors.push_back("File too large: " + safe_filename.string());
                continue;
            }

            // Validate content type matches extension (basic check)
            if (!part.content_type.empty()) 
            {
                bool type_mismatch = false;
                std::string ext = safe_filename.extension().string();

                if ((ext == ".jpg" || ext == ".jpeg") &&
                    part.content_type.find("image/jpeg") == std::string::npos) {
                    type_mismatch = true;
                }
                else if (ext == ".png" &&
                    part.content_type.find("image/png") == std::string::npos) {
                    type_mismatch = true;
                }
                else if (ext == ".pdf" &&
                    part.content_type.find("application/pdf") == std::string::npos) {
                    type_mismatch = true;
                }

                if (type_mismatch) {
                    logger.warning("Content-Type mismatch for file: " + safe_filename.string());
                }
            }

            // Get target directory from form field
            std::string target_dir;
            for (const auto& p : parts) {
                if (!p.isFile() && p.name == "directory") {
                    target_dir = p.data;
                    // Sanitize directory path
                    std::replace(target_dir.begin(), target_dir.end(), '\\', '/');
                    if (target_dir.find("..") != std::string::npos) {
                        target_dir = "";  // Reset if contains parent directory reference
                    }
                    break;
                }
            }

            // Construct the full path
            std::filesystem::path full_path = std::filesystem::path(config.rootDir);
            if (!target_dir.empty()) {
                full_path /= target_dir;
            }
            full_path /= safe_filename;

            // Security check: ensure the path is within root directory
            auto canonical_root = std::filesystem::canonical(config.rootDir);
            auto target_path = std::filesystem::weakly_canonical(full_path);

            if (!target_path.string().starts_with(canonical_root.string())) {
                errors.push_back("Invalid path for file: " + safe_filename.string());
                continue;
            }

            // Check if file exists and handle accordingly
            if (std::filesystem::exists(full_path) && !config.allow_overwrite) {
                // Generate unique filename
                std::string base = safe_filename.stem().string();
                std::string ext = safe_filename.extension().string();
                int counter = 1;

                while (std::filesystem::exists(full_path)) {
                    safe_filename = base + "_" + std::to_string(counter) + ext;
                    full_path = full_path.parent_path() / safe_filename;
                    counter++;
                }
            }

            // Create directories if needed
            std::filesystem::create_directories(full_path.parent_path());

            // First write to temp file
            std::filesystem::path temp_path = std::filesystem::path(config.upload_temp_dir) /
                (safe_filename.string() + ".tmp");

            {
                std::ofstream temp_file(temp_path, std::ios::binary);
                if (!temp_file) {
                    errors.push_back("Failed to create temp file: " + safe_filename.string());
                    continue;
                }
                temp_file.write(part.data.data(), part.data.size());
            }

            // Move from temp to final location (atomic on most filesystems)
            std::filesystem::rename(temp_path, full_path);

            // Set appropriate permissions (Unix-like systems)
#ifndef _WIN32
            std::filesystem::permissions(full_path,
                std::filesystem::perms::owner_read |
                std::filesystem::perms::owner_write |
                std::filesystem::perms::group_read |
                std::filesystem::perms::others_read);
#endif

            // Get file info for response
            json file_info = {
                {"filename", safe_filename.string()},
                {"original_filename", part.filename},
                {"size", part.data.size()},
                {"content_type", part.content_type},
                {"path", std::filesystem::relative(full_path, config.rootDir).string()},
                {"uploaded_at", std::chrono::system_clock::now().time_since_epoch().count()}
            };

            uploaded_files.push_back(file_info);

            logger.info("Uploaded file: " + full_path.string() + " (" +
                std::to_string(part.data.size()) + " bytes)");

        }
        catch (const std::filesystem::filesystem_error& e) 
        {
            errors.push_back("Filesystem error for " + part.filename + ": " + e.what());
            logger.error("Filesystem error: " + std::string(e.what()));
        }
        catch (const std::exception& e) 
        {
            errors.push_back("Error uploading " + part.filename + ": " + e.what());
            logger.error("Upload error: " + std::string(e.what()));
        }
    }

    // Clean up temp directory
    try 
    {
        for (const auto& entry : std::filesystem::directory_iterator(config.upload_temp_dir)) 
        {
            if (entry.path().extension() == ".tmp") 
            {
                // Remove old temp files (older than 1 hour)
                auto age = std::filesystem::last_write_time(entry.path()).time_since_epoch();
                auto now = std::filesystem::file_time_type::clock::now().time_since_epoch();
                if ((now - age) > std::chrono::hours(1)) 
                {
                    std::filesystem::remove(entry.path());
                }
            }
        }
    }
    catch (...) 
    {
        // Ignore cleanup errors
    }

    // Prepare response
    json result = 
    {
        {"success", !uploaded_files.empty()},
        {"uploaded", uploaded_files},
        {"total_files", file_count},
        {"successful_uploads", uploaded_files.size()}
    };

    if (!errors.empty()) 
    {
        result["errors"] = errors;
    }

    if (uploaded_files.empty() && !errors.empty()) 
    {
        response.setError(400, "Upload failed - no files were uploaded successfully");
        std::string json_str = result.dump(2);
        response.body.assign(json_str.begin(), json_str.end());
        response.headers["Content-Type"] = "application/json";
    }
    else 
    {
        response.setJson(result);
    }

    return response;
}