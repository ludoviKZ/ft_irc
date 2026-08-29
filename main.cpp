#include "Server.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        std::cerr << "Usage: ./ircserv <port> <password>" << std::endl;
        return 1;
    }

    char* end = NULL;
    long port = std::strtol(argv[1], &end, 10);
    if (end == argv[1] || *end != '\0' || port < 1 || port > 65535)
    {
        std::cerr << "Error: invalid port number." << std::endl;
        return 1;
    }

    try
    {
        Server server(static_cast<int>(port), argv[2]);
        server.start();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Server startup failed: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
