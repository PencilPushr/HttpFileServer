#pragma once

#include <string>
#include <map>
#include <vector>
#include <sstream>
#include <iostream>

class HttpRequest {
public:
    std::string method;
    std::string path;
    std::string query_string;
    std::map<std::string, std::string> headers;
    std::vector<uint8_t> body;
    std::string client_ip;

    static HttpRequest parse(const std::string& raw_request, const std::string& client_ip = "");

    std::map<std::string, std::string> parseQuery() const;

private:
    std::string urlDecode(const std::string& str) const;
};