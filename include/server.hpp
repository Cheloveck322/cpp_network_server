#ifndef SERVER_HPP
#define SERVER_HPP

#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <atomic>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include "client.hpp"

struct ServerStats 
{
    uint64_t total_connections;
    uint64_t current_connections;
    uint64_t total_messages;
    uint64_t total_commands;
    std::chrono::seconds uptime;
};

class NetworkServer 
{
public:
    NetworkServer(int tcp_port = 8080, int udp_port = 8081);
    ~NetworkServer();

    bool initialize();
    void run();
    void shutdown();

private:
    int createTcpSocket();
    int createUdpSocket();
    bool setupEpoll();

    void handleTcpConnection();
    void handleTcpData(int client_fd);
    void handleUdpData();
    void processClientMessage(int client_fd, const std::string& message, bool is_udp = false,
                            struct sockaddr_in* udp_addr = nullptr, socklen_t udp_addr_len = 0);

    std::string processCommand(const std::string& command);
    std::string getCurrentTime();
    std::string getStats();

    void removeClient(int client_fd);
    bool setNonBlocking(int fd);
    void sendResponse(int client_fd, const std::string& response, bool is_udp = false,
                     struct sockaddr_in* udp_addr = nullptr, socklen_t udp_addr_len = 0);

private:
    int _tcp_port;
    int _udp_port;
    
    int _tcp_socket;
    int _udp_socket;
    int _epoll_fd;
    
    std::unordered_map<int, std::unique_ptr<ClientInfo>> _clients;
    std::unordered_set<std::string> _udp_clients; 
    
    std::atomic<uint64_t> _total_connections;
    std::atomic<uint64_t> _current_connections;
    std::chrono::system_clock::time_point _start_time;
    
    std::atomic<bool> _running;
    static constexpr int MAX_EVENTS = 64;
    static constexpr int BUFFER_SIZE = 4096;
};

#endif // SERVER_HPP