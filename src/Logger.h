#pragma once

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