#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <chrono>
#include <cstdint>

struct ClientInfo 
{
    std::string address;
    uint16_t port;
    std::chrono::system_clock::time_point connect_time;
    uint64_t bytes_received;
    uint64_t bytes_sent;
    
    ClientInfo(const std::string& addr, uint16_t p) 
        : address(addr), port(p), 
          connect_time(std::chrono::system_clock::now()),
          bytes_received(0), bytes_sent(0) {}
};

#endif //CLIENT_HPP