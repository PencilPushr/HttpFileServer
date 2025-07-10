#pragma once
#include <string>
#include <vector>
#include <map>
#include <sstream>

class MultipartParser {
public:
    struct Part {
        std::map<std::string, std::string> headers;
        std::string name;
        std::string filename;
        std::string content_type;
        std::string data;

        bool isFile() const { return !filename.empty(); }
    };

    static std::vector<Part> parse(const std::string& body, const std::string& boundary);

private:
    static std::string trim(const std::string& str);
    static std::map<std::string, std::string> parseHeaders(const std::string& header_block);
    static void parseContentDisposition(const std::string& value, Part& part);
};