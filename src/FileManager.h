#pragma once

#include <string>
#include <mutex>
#include <cstdint>

#include "Logger.h"

#include "json.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;


class FileManager
{
private:
    std::string root_directory;
    std::mutex file_mutex;
    Logger& logger;

public:
    struct FileInfo
    {
        std::string name;
        std::string path;
        uint64_t size;
        std::string modified;
        bool is_directory;
        std::string mime_type;

        json toJson() const;
    };

    FileManager(const std::string& root, Logger& log);
    std::vector<FileInfo> listDirectory(const std::string& relative_path = "");
    std::vector<uint8_t> readFile(const std::string& relative_path);
    bool writeFile(const std::string& relative_path, const std::vector<uint8_t>& data);
    bool deleteFile(const std::string& relative_path);
    json getStats();

private:
    bool isPathSafe(const std::string& path) const;
    std::string getMimeType(const std::string& filename) const;
    static std::string formatFileSize(uint64_t size);
};

