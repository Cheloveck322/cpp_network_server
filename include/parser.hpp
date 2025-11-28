#ifndef PARSER_HPP
#define PARSER_HPP

#include <string>

struct CommandLineArgs 
{
    int tcp_port = 8080;
    int udp_port = 8081;
    bool show_help = false;
    bool error = false;
    std::string error_msg;
};

CommandLineArgs parseArguments(int argc, char* argv[]);
void printUsage(const char* program_name);

#endif // PARSER_HPP