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

/*
#define LOG(msg) (Log(__FILE__, __LINE__, LogData<None>() << msg))

// Workaround GCC 4.7.2 not recognizing noinline attribute
#ifndef NOINLINE_ATTRIBUTE
  #ifdef __ICC
    #define NOINLINE_ATTRIBUTE __attribute__(( noinline ))
  #else
    #define NOINLINE_ATTRIBUTE
  #endif // __ICC
#endif // NOINLINE_ATTRIBUTE

template<typename List>
void Log(const char* file, int line,
         LogData<List>&& data) NOINLINE_ATTRIBUTE
{
    std::cout << file << ":" << line << ": ";
    output(std::cout, std::move(data.list));
    std::cout << std::endl;
}

struct None { };

template<typename List>
struct LogData {
    List list;
};

template<typename Begin, typename Value>
constexpr LogData<std::pair<Begin&&, Value&&>> operator<<(LogData<Begin>&& begin,
                                                          Value&& value) noexcept
{
    return {{ std::forward<Begin>(begin.list), std::forward<Value>(value) }};
}

template<typename Begin, size_t n>
constexpr LogData<std::pair<Begin&&, const char*>> operator<<(LogData<Begin>&& begin,
                                                              const char (&value)[n]) noexcept
{
    return {{ std::forward<Begin>(begin.list), value }};
}

typedef std::ostream& (*PfnManipulator)(std::ostream&);

template<typename Begin>
constexpr LogData<std::pair<Begin&&, PfnManipulator>> operator<<(LogData<Begin>&& begin,
                                                                 PfnManipulator value) noexcept
{
    return {{ std::forward<Begin>(begin.list), value }};
}

template <typename Begin, typename Last>
void output(std::ostream& os, std::pair<Begin, Last>&& data)
{
    output(os, std::move(data.first));
    os << data.second;
}

inline void output(std::ostream& os, None)
{ }
*/