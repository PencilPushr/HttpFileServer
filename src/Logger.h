#pragma once

#include <mutex>
#include <fstream>
#include <string>


class Logger {
private:
    std::ofstream log_file;
    std::mutex log_mutex;
    bool enabled;

public:
    Logger(const std::string& filename, bool enable = true);

    void log(const std::string& level, const std::string& message);

    void info(const std::string& msg);
    void error(const std::string& msg);
    void warning(const std::string& msg);
};