#pragma once

class StaticFileServer 
{
public:
    StaticFileServer(const std::string& web_dir, Logger& log);
    HttpResponse serveFile(const std::string& path);

private:
    std::string web_directory;
    Logger& logger;
    std::string getMimeType(const std::string& filename);
};