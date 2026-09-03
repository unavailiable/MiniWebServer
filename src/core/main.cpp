#include "../../include/core/server.hpp"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <port>" << std::endl;
        return 1;
    }

    try
    {
        int port = std::stoi(argv[1]);

        if (port <= 0 || port > 65535)
        {
            throw std::invalid_argument("port must be between 1 and 65535");
        }

        Server server(port, "./resources", 8, 10000);

        std::cout << "MiniWebServer listening on port " << port << std::endl;

        server.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "server error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}