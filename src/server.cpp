#include "../include/server.hpp"
#include <iostream>
#include <csignal>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <array>
#include <arpa/inet.h>
#include <sstream>
#include <iomanip>

NetworkServer* g_server_instance{ nullptr }; //global server instance for signal handling

void signalHandler(int signum)
{
    if (g_server_instance) 
    {
        std::cout << "\n[INFO] Received signal " << signum << ", shutting down..." << std::endl;
        g_server_instance->shutdown();
    }
}

NetworkServer::NetworkServer(int tcp_port, int udp_port)
    :   _tcp_port{ tcp_port }, _udp_port{ udp_port },
        _tcp_socket{ -1 }, _udp_socket{ -1 }, _epoll_fd{ -1 },
        _total_connections{ 0 }, _current_connections{ 0 },
        _running{ false }, _start_time{ std::chrono::system_clock::now() }
{
    g_server_instance = this;
}

NetworkServer::~NetworkServer()
{
    shutdown();
    g_server_instance = nullptr;
}

bool NetworkServer::initialize()
{
    std::cout << "[INFO] Initializing network server..." << std::endl;

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    signal(SIGPIPE, SIG_IGN);

    _tcp_socket = createTcpSocket();
    if (_tcp_socket < 0)
    {
        std::cerr << "[ERROR] Failed to create TCP socket." << std::endl;
        return false;
    }

    _udp_socket = createUdpSocket();
    if (_udp_socket < 0)
    {
        std::cerr << "[ERROR] Failed to create UDP socket." << std::endl;
        return false;
    }

    if (!setupEpoll())
    {
        std::cerr << "[ERROR] Failed to setup epoll." << std::endl;
        return false;
    }

    std::cout << "[INFO] Server initialized successfully" << std::endl;
    std::cout << "[INFO] TCP listening on port " << _tcp_port << std::endl;
    std::cout << "[INFO] UDP listening on port " << _udp_port << std::endl;
    
    return true;
}

int NetworkServer::createTcpSocket()
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("socket");
        return -1;
    }

    int opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
    {
        perror("setsockopt SO_REUSEADDR");
        close(sock);
        return -1;
    }

    if (!setNonBlocking(sock))
    {
        close(sock);
        return -1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(_tcp_port);

    if (bind(sock, (sockaddr*)&addr, sizeof(addr)) < 0)
    {
        perror("bind TCP");
        close(sock);
        return -1;
    }

    if (listen(sock, SOMAXCONN) < 0)
    {
        perror("listen");
        close(sock);
        return -1;
    }

    return sock;
}

int NetworkServer::createUdpSocket()
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        perror("socket UDP");
        return -1;
    }

    int opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt SO_REUSEADDR UDP");
        close(sock);
        return -1;
    }

    if (!setNonBlocking(sock))
    {
        close(sock);
        return -1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(_udp_port);

    if (bind(sock, (sockaddr*)&addr, sizeof(addr)) < 0)
    {
        perror("bind UDP");
        close(sock);
        return -1;
    }

    return sock;
}

bool NetworkServer::setupEpoll()
{
    _epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (_epoll_fd < 0)
    {
        perror("epoll_create1");
        return false;
    }

    epoll_event event{};
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = _tcp_socket;

    if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, _tcp_socket, &event) < 0)
    {
        perror("epoll_ctl TCP");
        return false;
    }

    event.events = EPOLLIN | EPOLLET;
    event.data.fd = _udp_socket;

    if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, _udp_socket, &event) < 0)
    {
        perror("epoll_ctl UDP");
        return false;
    }

    return true;
}

bool NetworkServer::setNonBlocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
    {
        perror("fcntl F_GETFL");
        return false;
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        perror("fcntl F_SETFL");
        return false;
    }

    return true;
}

void NetworkServer::run()
{
    _running = true;
    std::array<epoll_event, MAX_EVENTS> events;

    std::cout << "[INFO] Server is running. Press Ctrl+C to stop." << std::endl;

    while (_running)
    {
        int nfds = epoll_wait(_epoll_fd, events.data(), MAX_EVENTS, 100);

        if (nfds < 0)
        {
            if (errno == EINTR)
                continue;
            perror("epoll_wain");
            break;
        }

        for (int i = 0; i < nfds; ++i)
        {
            if (events[i].data.fd == _tcp_socket)
            {
                handleTcpConnection();
            }
            else if (events[i].data.fd == _udp_socket)
            {
                handleUdpData();
            }
            else
            {
                if (events[i].events & (EPOLLHUP | EPOLLERR))
                {
                    removeClient(events[i].data.fd);
                }
                else if (events[i].events & EPOLLIN)
                {
                    handleTcpData(events[i].data.fd);
                }
            }
        }
    }

    std::cout << "[INFO] Server stopped" << std::endl;
}

void NetworkServer::handleTcpConnection()
{
    while (true)
    {
        sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);

        int client_fd = accept(_tcp_socket, (sockaddr*)&client_addr, &addr_len);
        if (client_fd < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK) 
            {
                break;  // No more pending connections
            }
            perror("accept");
            continue;
        }

        if (!setNonBlocking(client_fd))
        {
            close(client_fd);
            continue;
        }

        epoll_event event{};
        event.events = EPOLLIN | EPOLLET | EPOLLHUP | EPOLLERR;
        event.data.fd = client_fd;

        if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, client_fd, &event) < 0)
        {
            perror("epoll_ctl client");
            close(client_fd);
            continue;
        }

        std::string client_ip = inet_ntoa(client_addr.sin_addr);
        uint16_t client_port = ntohs(client_addr.sin_port);

        _clients[client_fd] = std::make_unique<ClientInfo>(client_ip, client_port);
        ++_total_connections;
        ++_current_connections;

        std::cout << "[INFO] New TCP connection from " << client_ip << ":" << client_port 
                  << " (fd: " << client_fd << ")" << std::endl;
    }
}

void NetworkServer::handleTcpData(int client_fd)
{
    std::array<char, BUFFER_SIZE> buffer;
    std::string accumulated_data;

    while (true)
    {
        ssize_t bytes = recv(client_fd, buffer.data(), sizeof(buffer) - 1, 0);

        if (bytes < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                if (!accumulated_data.empty())
                {
                    processClientMessage(client_fd, accumulated_data);
                }
                break;
            }
            perror("recv");
            removeClient(client_fd);
            return;
        }
        else if (bytes == 0)
        {
            removeClient(client_fd);
            return;
        }

        buffer[bytes] = '\0';
        
        if (_clients.find(client_fd) != _clients.end())
        {
            _clients[client_fd]->bytes_received += bytes;
        }

        accumulated_data.append(buffer.data(), bytes);

        size_t pos;
        while ((pos = accumulated_data.find('\n')) != std::string::npos)
        {
            std::string message = accumulated_data.substr(0, pos);

            if (!message.empty() && message.back() == '\r')
            {
                message.pop_back();
            }

            processClientMessage(client_fd, message);
            accumulated_data.erase(0, pos + 1);
        }
    }
}

void NetworkServer::handleUdpData()
{
    std::array<char, BUFFER_SIZE> buffer;
    sockaddr_in client_addr{};
    socklen_t addr_len = sizeof(client_addr);

    while (true)
    {
        ssize_t bytes = recvfrom(_udp_socket, buffer.data(), sizeof(buffer) - 1, 0,
                                (sockaddr*)&client_addr, &addr_len);

        if (bytes < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                break;
            }
            perror("recvfrom");
            continue;
        }

        buffer[bytes] = '\0';

        std::string client_key = std::string(inet_ntoa(client_addr.sin_addr)) + ":" + 
                                std::to_string(ntohs(client_addr.sin_port));
                                
        if (_udp_clients.find(client_key) == _udp_clients.end())
        {
            _udp_clients.insert(client_key);
            ++_total_connections;
            std::cout << "[INFO] New UDP client: " << client_key << std::endl;
        }

        std::string message(buffer.data(), bytes);
        while (!message.empty() && (message.back() == '\n' || message.back() == '\r'))
        {
            message.pop_back();
        }

        processClientMessage(-1, message, true, &client_addr, addr_len);
    }
}

void NetworkServer::processClientMessage(int client_fd, const std::string& message, bool is_udp,
                                        struct sockaddr_in* udp_addr, socklen_t udp_addr_len)
{
    if (message.empty()) return;

    std::string response;

    if (message[0] == '/')
    {
        response = processCommand(message);

        if (message == "/shutdown")
        {
            sendResponse(client_fd, response, is_udp, udp_addr, udp_addr_len);
            shutdown();
            return;
        }
    }
    else
    {
        response = message;
    }

    sendResponse(client_fd, response, is_udp, udp_addr, udp_addr_len);
}

std::string NetworkServer::processCommand(const std::string& command)
{
    if (command == "/time")
    {
        return getCurrentTime();
    }
    else if (command == "/stats")
    {
        return getStats();
    }
    else if (command == "/shutdown")
    {
        return "The server is shutting down...";
    }
    else 
    {
        return "Unknown command: " + command;
    }
}

std::string NetworkServer::getCurrentTime()
{
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::string NetworkServer::getStats()
{
    auto now = std::chrono::system_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - _start_time);

    std::stringstream ss;
    ss << "Server Statistics:\n";
    ss << "Total connections: " << _total_connections.load() << "\n";
    ss << "Current TCP connections: " << _current_connections.load() << "\n";
    ss << "Current UDP clients: " << _udp_clients.size() << "\n";
    ss << "Uptime: " << uptime.count() << " seconds";
    
    return ss.str();
}

void NetworkServer::sendResponse(int client_fd, const std::string& response, bool is_udp,
                                sockaddr_in* udp_addr, socklen_t udp_addr_len)
{
    if (is_udp && udp_addr)
    {
        std::string data = response + "\n";
        ssize_t sent = sendto(_udp_socket, data.c_str(), data.length(), 0,
                            (sockaddr*)udp_addr, udp_addr_len);
        if (sent < 0)
        {
            perror("sendto");
        }
    }
    else if (client_fd >= 0)
    {
        std::string data = response + "\n";
        ssize_t total_sent = 0;

        while (total_sent < static_cast<ssize_t>(data.length()))
        {
            ssize_t sent = send(client_fd, data.c_str() + total_sent, 
                                data.length() - total_sent, MSG_NOSIGNAL);

            if (sent < 0)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK) 
                {
                    break;
                }
                perror("send");
                break;
            }

            total_sent += sent;

            if (_clients.find(client_fd) != _clients.end())
            {
                _clients[client_fd]->bytes_sent += sent;
            }
        }
    }
}

void NetworkServer::removeClient(int client_fd)
{
    auto it = _clients.find(client_fd);
    if (it != _clients.end())
    {
        std::cout << "[INFO] Client disconnected: " << it->second->address 
                  << ":" << it->second->port << " (fd: " << client_fd << ")" << std::endl;
        _clients.erase(it);
        --_current_connections;
    }

    epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
    close(client_fd);
}

void NetworkServer::shutdown()
{
    _running = false;

    for (auto& [fd, client] : _clients)
    {
        close(fd);
    }

    if (_tcp_socket >= 0)
    {
        close(_tcp_socket);
        _tcp_socket = -1;
    }

    if (_udp_socket >= 0)
    {
        close(_udp_socket);
        _udp_socket = -1;
    }

    if (_epoll_fd >= 0)
    {
        close(_epoll_fd);
        _epoll_fd = -1;
    }
}