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
    mutable std::mutex file_mutex;
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

    struct UploadResult 
    {
        bool success;
        std::string relative_path;
        std::string final_filename;
        uint64_t size;
        std::string error;
    };

    FileManager(const std::string& root, Logger& log);

    // Upload file with automatic renaming if needed
    UploadResult uploadFile(const std::string& filename,
        const std::vector<uint8_t>& data,
        const std::string& target_dir = "",
        bool allow_overwrite = false);

    // Upload file from string data
    UploadResult uploadFile(const std::string& filename,
        const std::string& data,
        const std::string& target_dir = "",
        bool allow_overwrite = false);

    std::vector<FileInfo> listDirectory(const std::string& relative_path = "");
    std::vector<uint8_t> readFile(const std::string& relative_path);
    bool writeFile(const std::string& relative_path, const std::vector<uint8_t>& data);
    bool deleteFile(const std::string& relative_path);
    json getStats();
    bool fileExists(const std::string& relative_path) const;
    std::string generateUniqueFilename(const std::string& relative_path) const;

private:
    bool isPathSafe(const std::string& path) const;
    std::string getMimeType(const std::string& filename) const;
    static std::string formatFileSize(uint64_t size);
};

