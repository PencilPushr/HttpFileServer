// RaspberryServer.cpp : Defines the entry point for the application.
//

#include <iostream>

int main() {
    try {
        Config config;
        Server server(config);

        std::cout << "Starting file server..." << std::endl;
        std::cout << "Web interface: http://localhost:" << config.port << std::endl;
        std::cout << "API base URL: http://localhost:" << config.port << "/api/" << std::endl;

        server.start();
    }
    catch (const std::exception& e) 
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}