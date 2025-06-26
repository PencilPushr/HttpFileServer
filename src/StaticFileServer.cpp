#include "StaticFileServer.h"

#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

StaticFileServer::StaticFileServer(const std::string& webDir, Logger& log)
    : webDirectory(webDir)
    , logger(log)
{}

HttpResponse StaticFileServer::serveFile(const std::string& path)
{
    HttpResponse response;

    std::string file_path = this->webDirectory;
    if (path == "/") 
    {
        file_path += "/index.html";
    }
    else 
    {
        file_path += path;
    }

    // Debug logging
    logger.info("Attempting to serve: " + file_path);
    logger.info("File exists: " + std::string(fs::exists(file_path) ? "YES" : "NO"));

    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        response.status_code = 404;
        response.setError(404, "File not found");
        logger.warning("Static file not found: " + file_path);
        return response;
    }

    std::vector<uint8_t> content((std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());

    response.body = content;
    response.headers["Content-Type"] = getMimeType(file_path);
    response.headers["Cache-Control"] = "no-cache";

    logger.info("Served static file: " + file_path);
    return response;
}

std::string StaticFileServer::getMimeType(const std::string& filename)
{
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

