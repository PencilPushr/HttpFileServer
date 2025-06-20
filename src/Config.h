#pragma once

struct Config 
{
    int port = 8080;
    std::string root_directory = "./files";
    std::string web_directory = "./web";
    bool enable_cors = true;
    bool enable_logging = true;
    std::string log_file = "server.log";
};
