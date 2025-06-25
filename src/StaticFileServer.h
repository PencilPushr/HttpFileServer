#pragma once

#include <string>

#include "Logger.h"
#include "HTTP/HttpResponse.h"

class StaticFileServer {
private:
    std::string webDirectory;
    Logger& logger;

public:
    StaticFileServer(const std::string& web_dir, Logger& log);

    HttpResponse serveFile(const std::string& path);

private:
    std::string getMimeType(const std::string& filename);
};
