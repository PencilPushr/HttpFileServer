#include "FileManager.h"

#include <iomanip>
#include <sstream>
#include <chrono>

FileManager::FileManager(const std::string& root, Logger& log)
    : root_directory(root)
    , logger(log)
{
    fs::create_directories(root);
    logger.info("FileManager initialized with root: " + root);
}

json FileManager::FileInfo::toJson() const
{
    return json{
        {"name", name},
        {"path", path},
        {"size", size},
        {"modified", modified},
        {"is_directory", is_directory},
        {"mime_type", mime_type},
        {"size_formatted", FileManager::formatFileSize(size)}
    };
}

std::vector<FileManager::FileInfo> FileManager::listDirectory(const std::string& relative_path) {
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

std::vector<uint8_t> FileManager::readFile(const std::string& relative_path) {
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

bool FileManager::writeFile(const std::string& relative_path, const std::vector<uint8_t>& data) {
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

bool FileManager::deleteFile(const std::string& relative_path) {
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

json FileManager::getStats() {
    std::lock_guard<std::mutex> lock(file_mutex);

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

    return json{
        {"total_files", file_count},
        {"total_folders", folder_count},
        {"total_size", total_size},
        {"total_size_formatted", formatFileSize(total_size)}
    };
}

bool FileManager::fileExists(const std::string& relative_path) const
{
    std::lock_guard<std::mutex> lock(file_mutex);
    std::string full_path = root_directory + "/" + relative_path;

    if (!isPathSafe(full_path)) {
        return false;
    }

    return std::filesystem::exists(full_path);
}

std::string FileManager::generateUniqueFilename(const std::string& relative_path) const
{
    if (!fileExists(relative_path)) {
        return relative_path;
    }

    std::filesystem::path path(relative_path);
    std::string dir = path.parent_path().string();
    std::string base = path.stem().string();
    std::string ext = path.extension().string();

    int counter = 1;
    std::string new_path;

    do {
        std::string new_filename = base + "_" + std::to_string(counter) + ext;
        if (!dir.empty()) {
            new_path = dir + "/" + new_filename;
        }
        else {
            new_path = new_filename;
        }
        counter++;
    } while (fileExists(new_path) && counter < 1000);

    return new_path;
}

bool FileManager::isPathSafe(const std::string& path) const {
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

std::string FileManager::getMimeType(const std::string& filename) const {
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

std::string FileManager::formatFileSize(uint64_t size) {
    const char* units[] = { "B", "KB", "MB", "GB", "TB" };
    int unit = 0;
    double s = static_cast<double>(size);

    while (s >= 1024 && unit < 4) {
        s /= 1024;
        unit++;
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << s << " " << units[unit];
    return oss.str();
}


FileManager::UploadResult FileManager::uploadFile(const std::string& filename,
    const std::vector<uint8_t>& data,
    const std::string& target_dir,
    bool allow_overwrite)
{
    UploadResult result;
    result.size = data.size();

    // Sanitize filename
    std::filesystem::path safe_filename = std::filesystem::path(filename).filename();

    // Validate filename
    if (safe_filename.string().empty() || safe_filename.string().find("..") != std::string::npos) {
        result.success = false;
        result.error = "Invalid filename";
        return result;
    }

    // Build relative path
    std::string relative_path;
    if (!target_dir.empty()) {
        // Sanitize target directory
        std::string safe_dir = target_dir;
        std::replace(safe_dir.begin(), safe_dir.end(), '\\', '/');
        // Remove leading/trailing slashes
        if (!safe_dir.empty() && safe_dir.front() == '/') {
            safe_dir = safe_dir.substr(1);
        }
        if (!safe_dir.empty() && safe_dir.back() == '/') {
            safe_dir.pop_back();
        }

        if (safe_dir.find("..") != std::string::npos) {
            result.success = false;
            result.error = "Invalid directory path";
            return result;
        }

        relative_path = safe_dir + "/" + safe_filename.string();
    }
    else {
        relative_path = safe_filename.string();
    }

    // Handle existing files
    if (!allow_overwrite && fileExists(relative_path)) {
        relative_path = generateUniqueFilename(relative_path);
        safe_filename = std::filesystem::path(relative_path).filename();
    }

    // Write the file
    bool success = writeFile(relative_path, data);

    result.success = success;
    result.relative_path = relative_path;
    result.final_filename = safe_filename.string();

    if (!success) {
        result.error = "Failed to write file";
    }

    return result;
}

FileManager::UploadResult FileManager::uploadFile(const std::string& filename,
    const std::string& data,
    const std::string& target_dir,
    bool allow_overwrite)
{
    // Convert string to vector<uint8_t>
    std::vector<uint8_t> binary_data(data.begin(), data.end());
    return uploadFile(filename, binary_data, target_dir, allow_overwrite);
}