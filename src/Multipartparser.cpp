#include "MultipartParser.h"
#include <algorithm>
#include <regex>

std::string MultipartParser::trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

std::map<std::string, std::string> MultipartParser::parseHeaders(const std::string& header_block) {
    std::map<std::string, std::string> headers;
    std::istringstream stream(header_block);
    std::string line;

    while (std::getline(stream, line)) {
        if (line.empty() || line == "\r") continue;

        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = trim(line.substr(0, colon));
            std::string value = trim(line.substr(colon + 1));

            // Convert to lowercase for case-insensitive lookup
            std::transform(key.begin(), key.end(), key.begin(), ::tolower);
            headers[key] = value;
        }
    }

    return headers;
}

void MultipartParser::parseContentDisposition(const std::string& value, Part& part) {
    // Parse: form-data; name="field_name"; filename="file.txt"
    std::regex name_regex("name=\"([^\"]+)\"");
    std::regex filename_regex("filename=\"([^\"]+)\"");

    std::smatch matches;
    if (std::regex_search(value, matches, name_regex)) {
        part.name = matches[1];
    }

    if (std::regex_search(value, matches, filename_regex)) {
        part.filename = matches[1];
    }
}

std::vector<MultipartParser::Part> MultipartParser::parse(const std::string& body, const std::string& boundary) {
    std::vector<Part> parts;

    // The actual boundary in the body includes "--" prefix
    std::string delimiter = "--" + boundary;
    std::string end_delimiter = delimiter + "--";

    size_t pos = 0;
    size_t boundary_pos = body.find(delimiter, pos);

    while (boundary_pos != std::string::npos) {
        // Move past the boundary and CRLF
        pos = boundary_pos + delimiter.length();
        if (pos < body.length() && body[pos] == '\r') pos++;
        if (pos < body.length() && body[pos] == '\n') pos++;

        // Find the next boundary
        size_t next_boundary = body.find("\r\n" + delimiter, pos);
        if (next_boundary == std::string::npos) {
            // Check for end delimiter
            next_boundary = body.find("\r\n" + end_delimiter, pos);
            if (next_boundary == std::string::npos) break;
        }

        // Extract the part content
        std::string part_content = body.substr(pos, next_boundary - pos);

        // Find the double CRLF that separates headers from data
        size_t header_end = part_content.find("\r\n\r\n");
        if (header_end == std::string::npos) {
            header_end = part_content.find("\n\n");
            if (header_end == std::string::npos) continue;
        }

        Part part;

        // Parse headers
        std::string header_block = part_content.substr(0, header_end);
        part.headers = parseHeaders(header_block);

        // Extract common headers
        auto cd_it = part.headers.find("content-disposition");
        if (cd_it != part.headers.end()) {
            parseContentDisposition(cd_it->second, part);
        }

        auto ct_it = part.headers.find("content-type");
        if (ct_it != part.headers.end()) {
            part.content_type = ct_it->second;
        }

        // Extract data (skip the double CRLF)
        size_t data_start = header_end + (part_content.substr(header_end, 4) == "\r\n\r\n" ? 4 : 2);
        part.data = part_content.substr(data_start);

        parts.push_back(std::move(part));

        // Move to next boundary
        boundary_pos = body.find(delimiter, next_boundary);
    }

    return parts;
}