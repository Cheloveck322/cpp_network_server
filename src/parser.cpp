#include <string>
#include <cstring>
#include <iostream>
#include "../include/parser.hpp"

CommandLineArgs parseArguments(int argc, char* argv[]) 
{
    CommandLineArgs args;

    for (int i = 1; i < argc; ++i) 
    {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") 
        {
            args.show_help = true;
            return args;
        }

        if (arg == "-t" || arg == "--tcp-port") 
        {
            if (i + 1 >= argc) 
            {
                args.error = true;
                args.error_msg = "Error: " + arg + " requires an argument";
                return args;
            }

            args.tcp_port = std::atoi(argv[++i]);
            if (args.tcp_port <= 0 || args.tcp_port > 65535) 
            {
                args.error = true;
                args.error_msg = "Error: Invalid TCP port number (must be 1-65535)";
                return args;
            }
            continue;
        }

        if (arg == "-u" || arg == "--udp-port") 
        {
            if (i + 1 >= argc) 
            {
                args.error = true;
                args.error_msg = "Error: " + arg + " requires an argument";
                return args;
            }

            args.udp_port = std::atoi(argv[++i]);
            if (args.udp_port <= 0 || args.udp_port > 65535) 
            {
                args.error = true;
                args.error_msg = "Error: Invalid UDP port number (must be 1-65535)";
                return args;
            }
            continue;
        }

        args.error = true;
        args.error_msg = "Error: Unknown option '" + arg + "'";
        return args;
    }

    if (args.tcp_port == args.udp_port) 
    {
        args.error = true;
        args.error_msg = "Error: TCP and UDP ports must be different";
        return args;
    }

    return args;
}

void printUsage(const char* program_name)
{
    std::cout << "Usage: " << program_name << " [options]\n"
              << "Options:\n"
              << "  -t, --tcp-port PORT    Set TCP port (default: 8080)\n"
              << "  -u, --udp-port PORT    Set UDP port (default: 8081)\n"
              << "  -h, --help             Show this help message\n"
              << "\nCommands supported by the server:\n"
              << "  /time      - Get current date and time\n"
              << "  /stats     - Get server statistics\n"
              << "  /shutdown  - Shutdown the server\n"
              << "\nExample:\n"
              << "  " << program_name << " --tcp-port 9090 --udp-port 9091\n";
}