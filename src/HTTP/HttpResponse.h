#pragma once

#include "json.hpp"  // Download from https://github.com/nlohmann/json
using json = nlohmann::json;

class HttpResponse {
public:
    int status_code = 200;
    std::map<std::string, std::string> headers;
    std::vector<uint8_t> body;

    HttpResponse() {
        // Default security headers
        headers["X-Frame-Options"] = "DENY";
        headers["X-Content-Type-Options"] = "nosniff";
        headers["X-XSS-Protection"] = "1; mode=block";
    }

    std::string serialize() const {
        std::ostringstream response;

        // Status line
        response << "HTTP/1.1 " << status_code << " " << getStatusText() << "\r\n";

        // Headers
        auto headers_copy = headers;
        headers_copy["Content-Length"] = std::to_string(body.size());
        headers_copy["Server"] = "RaspberryPi-FileServer/1.0";

        for (const auto& [key, value] : headers_copy) {
            response << key << ": " << value << "\r\n";
        }
        response << "\r\n";

        std::string header_str = response.str();
        std::vector<uint8_t> full_response(header_str.begin(), header_str.end());
        full_response.insert(full_response.end(), body.begin(), body.end());

        return std::string(full_response.begin(), full_response.end());
    }

    void setJson(const json& j) {
        std::string json_str = j.dump();
        body = std::vector<uint8_t>(json_str.begin(), json_str.end());
        headers["Content-Type"] = "application/json";
    }

    void setError(int code, const std::string& message) {
        status_code = code;
        json error = {
            {"error", message},
            {"status", code}
        };
        setJson(error);
    }

private:
    std::string getStatusText() const {
        switch (status_code) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 409: return "Conflict";
        case 413: return "Payload Too Large";
        case 500: return "Internal Server Error";
        default: return "Unknown";
        }
    }
};