#pragma once

#include <string>
#include <map>
#include <vector>
#include <sstream>

#include "json.hpp"
using json = nlohmann::json;

class HttpResponse {
public:
    int status_code = 200;
    std::map<std::string, std::string> headers;
    std::vector<uint8_t> body;

    HttpResponse();
    std::string serialize() const;
    void setJson(const json& j);
    void setError(int code, const std::string& message);

private:
    std::string getStatusText() const;
};