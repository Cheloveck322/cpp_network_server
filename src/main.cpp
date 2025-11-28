#include "../include/server.hpp"
#include "../include/parser.hpp"
#include <iostream>

int main(int argc, char* argv[])
{
    CommandLineArgs args = parseArguments(argc, argv);

    if (args.show_help) 
    {
        printUsage(argv[0]);
        return 0;
    }

    if (args.error) 
    {
        std::cerr << args.error_msg << "\n\n";
        printUsage(argv[0]);
        return 1;
    }
    
    std::cout << "\nNetwork Server Starting...\n";

    try
    {
        NetworkServer server(args.tcp_port, args.udp_port);
        
        if (!server.initialize()) 
        {
            std::cerr << "Failed to initialize server\n";
            return 1;
        }
        
        server.run();
        
    } 
    catch (const std::exception& e) 
    {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "Server terminated successfully\n";
    return 0;
}
