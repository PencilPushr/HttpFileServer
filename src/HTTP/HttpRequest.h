#pragma once

class HttpRequest {
public:
    std::string method;
    std::string path;
    std::string query_string;
    std::map<std::string, std::string> headers;
    std::vector<uint8_t> body;
    std::string client_ip;

    static HttpRequest parse(const std::string& raw_request, const std::string& client_ip = "") {
        HttpRequest req;
        req.client_ip = client_ip;
        std::istringstream stream(raw_request);
        std::string line;

        // Parse request line
        if (std::getline(stream, line)) {
            std::istringstream request_line(line);
            std::string full_path;
            request_line >> req.method >> full_path;

            // Split path and query string
            size_t query_pos = full_path.find('?');
            if (query_pos != std::string::npos) {
                req.path = full_path.substr(0, query_pos);
                req.query_string = full_path.substr(query_pos + 1);
            }
            else {
                req.path = full_path;
            }
        }

        // Parse headers
        while (std::getline(stream, line) && line != "\r") {
            size_t colon = line.find(':');
            if (colon != std::string::npos) {
                std::string key = line.substr(0, colon);
                std::string value = line.substr(colon + 2);
                if (!value.empty() && value.back() == '\r') {
                    value.pop_back();
                }
                req.headers[key] = value;
            }
        }

        // Read body if Content-Length is specified
        auto content_length_it = req.headers.find("Content-Length");
        if (content_length_it != req.headers.end()) {
            int content_length = std::stoi(content_length_it->second);
            std::string remaining_data((std::istreambuf_iterator<char>(stream)),
                std::istreambuf_iterator<char>());
            req.body = std::vector<uint8_t>(remaining_data.begin(), remaining_data.end());
        }

        return req;
    }

    std::map<std::string, std::string> parseQuery() const {
        std::map<std::string, std::string> params;
        std::istringstream stream(query_string);
        std::string pair;

        while (std::getline(stream, pair, '&')) {
            size_t equals = pair.find('=');
            if (equals != std::string::npos) {
                params[pair.substr(0, equals)] = urlDecode(pair.substr(equals + 1));
            }
        }
        return params;
    }

private:
    std::string urlDecode(const std::string& str) const {
        std::string result;
        for (size_t i = 0; i < str.length(); ++i) {
            if (str[i] == '%' && i + 2 < str.length()) {
                int value = std::stoi(str.substr(i + 1, 2), nullptr, 16);
                result += static_cast<char>(value);
                i += 2;
            }
            else if (str[i] == '+') {
                result += ' ';
            }
            else {
                result += str[i];
            }
        }
        return result;
    }
};